/* Copyright 2011 Filip Reesalu, Johan Gustafsson, Jonas Bornold
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "CPlugin.h"

// Includes
#include <stdexcept>
#include <webp/decode.h>
#include <cstring>
#include <cstdlib>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <fstream>

const std::string CPlugin::s_strPluginName("webp-npapi");
const std::string CPlugin::s_strPluginDescription(" (Image viewer for WebP)");
const std::string CPlugin::s_strPluginVersion("1.0.0.0");
	
NPNetscapeFuncs * CPlugin::s_pBrowserFunctions = NULL;

void CPlugin::setBrowserFunctions( NPNetscapeFuncs * const pBrowserFunctions )
{
	s_pBrowserFunctions = pBrowserFunctions;
}

const std::string & CPlugin::getPluginName()
{
	return s_strPluginName;
}

const std::string & CPlugin::getPluginDescription()
{
	return s_strPluginDescription;
}

const std::string & CPlugin::getPluginVersion()
{
	return s_strPluginVersion;
}

CPlugin::CPlugin( const NPP instance, const NPMIMEType mimeType, const uint16_t mode, const std::map<std::string, std::string> mapArgs, const NPSavedData * const saved )
	:	m_bHasSize( mapArgs.count("width") && mapArgs.count("height") ),
		m_bEmbedded( mode == NP_EMBED ),
		m_mapArgs( mapArgs ),
		m_npp(instance),
		m_pStream(NULL),
		m_pImageRawData(NULL),
		m_pImagePixbuf(NULL),
		m_pImageScaledPixbuf(NULL)
{
	#ifdef WEBPNPAPI_DEBUG
		printf("CPlugin::CPlugin() - Constructor starts!\n");
	#endif
	
	// Map data to src if it exists
	if( m_mapArgs.count("data") > 0 )
		m_mapArgs["src"] = m_mapArgs["data"];


	if( s_pBrowserFunctions == NULL )
		throw std::runtime_error("s_pBrowserFunctions not set!");

	// Make sure we can render this plugin
	NPBool browserSupportsWindowless = false;
	s_pBrowserFunctions->getvalue(instance, NPNVSupportsWindowless, &browserSupportsWindowless);
	if( !browserSupportsWindowless )
		throw std::runtime_error("Windowless mode not supported by the browser");

	s_pBrowserFunctions->setvalue(instance, NPPVpluginWindowBool, (void*) false);

	// Initialize mutexes
	if( pthread_mutex_init(&m_mutexImage, NULL) != 0 )
		throw std::runtime_error("Mutex initialization failed");
		
	if( pthread_mutex_init(&m_mutexStream, NULL) != 0 )
		throw std::runtime_error("Mutex initialization failed");
		
	#ifdef WEBPNPAPI_DEBUG
		printf("CPlugin::CPlugin() - Menu stuff naowz\n");
	#endif
		
	// Create new popup menu
	m_gtkMenu = gtk_menu_new();
	
	GtkWidget * const gtkItemSavePNG = gtk_menu_item_new_with_label("Save as PNG");
	GtkWidget * const gtkItemSaveWebP = gtk_menu_item_new_with_label("Save as WebP");
	GtkWidget * const gtkItemAbout = gtk_menu_item_new_with_label("About");
	
	g_signal_connect( gtkItemSavePNG, "activate", G_CALLBACK( saveAsPNG ), this );
	g_signal_connect( gtkItemSaveWebP, "activate", G_CALLBACK( saveAsWebP ), this );
	g_signal_connect( gtkItemAbout, "activate", G_CALLBACK( spawnAbout ), this );

	gtk_menu_shell_append( GTK_MENU_SHELL(m_gtkMenu), gtkItemSavePNG );
	gtk_menu_shell_append( GTK_MENU_SHELL(m_gtkMenu), gtkItemSaveWebP );
	gtk_menu_shell_append( GTK_MENU_SHELL(m_gtkMenu), gtkItemAbout );
	
	gtk_widget_show(gtkItemSavePNG);
	gtk_widget_show(gtkItemSaveWebP);
	gtk_widget_show(gtkItemAbout);
		
	#ifdef WEBPNPAPI_DEBUG
		printf("CPlugin::CPlugin() - Done\n");
	#endif
}

CPlugin::~CPlugin()
{
	// Remove menu
	gtk_widget_destroy(m_gtkMenu);
	
	
	#ifdef WEBPNPAPI_DEBUG
		printf("CPlugin::~CPlugin() - Destroying mutexes\n");
	#endif
	
	// Mutexes are not allowed to be locked at this point
	pthread_mutex_destroy(&m_mutexImage);
	pthread_mutex_destroy(&m_mutexStream);
	
	#ifdef WEBPNPAPI_DEBUG
		printf("CPlugin::~CPlugin() - g_object_unref(m_pImageScaledPixbuf)\n");
	#endif

	if( m_pImageScaledPixbuf != NULL );
		g_object_unref( m_pImageScaledPixbuf );
	
	#ifdef WEBPNPAPI_DEBUG
		printf("CPlugin::~CPlugin() - g_object_unref(m_pImagePixbuf)\n");
	#endif

	if( m_pImagePixbuf != NULL )
		g_object_unref( m_pImagePixbuf );
		
	#ifdef WEBPNPAPI_DEBUG
		printf("CPlugin::~CPlugin() - free(m_pImageRawData)\n");
	#endif	

	free(m_pImageRawData); // We use free() since it's allocated by libwebp
}

NPError CPlugin::setWindow(const NPWindow * const window)
{
	#ifdef WEBPNPAPI_DEBUG
		printf("CPlugin::setWindow() - Window set\n");
	#endif
	
	m_window = *window;
	return NPERR_NO_ERROR;
}

NPError CPlugin::newStream(const NPMIMEType mimeType, const NPStream * const stream, const NPBool seekable, uint16_t * const stype)
{
	if( pthread_mutex_lock(&m_mutexStream) == 0 )
	{
		// We should only ever accept one stream
		if( m_pStream == NULL && strcmp(mimeType, "image/webp") == 0)
		{
			// Pre-allocate memory if size is known
			if( stream->end > 0 )
				m_strStreamData.reserve( stream->end );

			m_pStream = stream;
			*stype = NP_NORMAL;
		}
		else
		{
			#ifdef WEBPNPAPI_DEBUG
				printf("CPlugin::newStream() - Ignoring invalid stream\n");
			#endif
		}
		
		const bool bAccepted = (m_pStream != NULL);
		pthread_mutex_unlock(&m_mutexStream);
		
		if( bAccepted )
			return NPERR_NO_ERROR;
		else
			return NPERR_GENERIC_ERROR;
	}
	else
	{
		#ifdef WEBPNPAPI_DEBUG
			printf("CPlugin::newStream() - Failed to lock stream mutex\n");
		#endif
		return NPERR_GENERIC_ERROR;
	}
}

NPError CPlugin::destroyStream(const NPStream * const stream, const NPReason reason)
{
	if( pthread_mutex_lock(&m_mutexStream) == 0 )
	{
		if( m_pStream == stream && reason == NPRES_DONE )
		{
			if( pthread_mutex_lock( &m_mutexImage ) == 0 )
			{
				int iWidth, iHeight;
				m_pImageRawData = WebPDecodeRGB( (uint8_t *)(m_strStreamData.c_str()),m_strStreamData.size(), &iWidth, &iHeight);		
				if( m_pImageRawData != NULL )
				{
					// Create pixbuf
					m_pImagePixbuf = gdk_pixbuf_new_from_data(
								m_pImageRawData, 
								GDK_COLORSPACE_RGB, 
								0, 8, iWidth, iHeight, iWidth * 3,
								  NULL, NULL );
								  
					#ifdef WEBPNPAPI_DEBUG
						printf("CPlugin::destroyStream() - Image decoded with size %ix%i, forcing redraw\n", iWidth, iHeight );
					#endif
					
					// Force redraw
					NPRect rect;
					rect.top = 0;
					rect.left = 0;
					rect.bottom = m_window.height;
					rect.right = m_window.width;
					
					s_pBrowserFunctions->invalidaterect(m_npp, &rect);
					s_pBrowserFunctions->forceredraw(m_npp);
				}
				else
				{
					#ifdef WEBPNPAPI_DEBUG
						printf("CPlugin::destroyStream() - Failed to decode image\n");
					#endif
				}
				
				pthread_mutex_unlock( &m_mutexImage );
			}
			else
			{
				#ifdef WEBPNPAPI_DEBUG
					printf("CPlugin::destroyStream() - Failed to lock image mutex\n");
				#endif
			}
		}
		else if( reason != NPRES_DONE )
		{
			#ifdef WEBPNPAPI_DEBUG
				printf("CPlugin::destroyStream() - Stream destroyed but not done\n");
			#endif		
		}
				
		pthread_mutex_unlock(&m_mutexStream);
		return NPERR_NO_ERROR;
	}
	else
	{
		#ifdef WEBPNPAPI_DEBUG
			printf("CPlugin::destroyStream() - Failed to lock stream mutex\n");
		#endif	
		return NPERR_GENERIC_ERROR;
	}
}

int32_t CPlugin::writeReady(const NPStream * const stream)
{
	int32_t returnSize = 0;
	
	if( pthread_mutex_lock(&m_mutexStream) == 0 )
	{
		if( stream->end > 0 )
			returnSize = stream->end - m_strStreamData.size();
		else
			returnSize = m_strStreamData.max_size() - m_strStreamData.size();
	
		pthread_mutex_unlock( &m_mutexStream );
	}
	else
	{
		#ifdef WEBPNPAPI_DEBUG
			printf("CPlugin::writeReady() - Failed to lock mutex stream\n");
		#endif	
	}

	#ifdef WEBPNPAPI_DEBUG
		printf("CPlugin::writeReady() - Accepting size %i bytes\n", returnSize);
	#endif		
	
	return returnSize;
}

int32_t CPlugin::write(const NPStream * const stream, const int32_t offset, const int32_t len, const void * const buffer)
{	
	if( pthread_mutex_lock(&m_mutexStream) == 0 )
	{
		int32_t returnLen = NPERR_GENERIC_ERROR;
		
		if( m_pStream == stream )
		{
			if( len > 0 )
				m_strStreamData.append( static_cast<const char *>(buffer), len );
			
			returnLen = len;
		}

		pthread_mutex_unlock(&m_mutexStream);
		
		#ifdef WEBPNPAPI_DEBUG
			printf("CPlugin::write() - Read %i bytes\n", returnLen);
		#endif
					
		return returnLen;
	}
	else
	{
		#ifdef WEBPNPAPI_DEBUG
			printf("CPlugin::write() - Failed to lock mutex stream\n");
		#endif		
		return 0;
	}
}

int16_t CPlugin::handleEvent(const void * const pEvent)
{
	const XEvent * const nativeEvent = static_cast<const XEvent * const>(pEvent);
	
	switch( nativeEvent->type )
	{
		case ButtonPress:
		{
			if( (nativeEvent->xbutton).button == Button3  )
				spawnPopup();
		}
		break;
		
		case GraphicsExpose:
		{
			const XGraphicsExposeEvent * const pExpose = &nativeEvent->xgraphicsexpose;
			
			//GdkNativeWindow nativeWinId = (XID)(pExpose->drawable);
			//GdkNativeWindow nativeWinId = (XID)(m_window.window);
			//GdkDrawable * const gdkWindow = GDK_DRAWABLE( gdk_x11_window_foreign_new_for_display(pExpose->display, pExpose->drawable) );
			//printf("drawable: %p\n", pExpose->drawable);
			GdkPixmap * const gdkPixmap = gdk_pixmap_foreign_new(pExpose->drawable);
						 
			if( gdkPixmap )
			{
				gdk_drawable_set_colormap( GDK_DRAWABLE(gdkPixmap), gdk_colormap_get_system() ); // Should the colormap be freed?
				drawWindow(gdkPixmap);
				g_object_unref(gdkPixmap);
			}
			else
			{
				#ifdef WEBPNPAPI_DEBUG
					printf("CPlugin::handleEvent() - No drawable\n");
				#endif
			}
		}		
		break;
		
		default:
			return 0;
		break;
	}

	return 1;
}

void CPlugin::drawWindow( GdkDrawable * const gdkDrawable )
{
	#ifdef WEBPNPAPI_DEBUG
		printf("CPlugin::drawWindow() - Start\n");
	#endif
					
	if(!m_npp)
		return;

	// Make sure we have a pixbuf before we draw anything
	if( pthread_mutex_trylock( &m_mutexImage ) == 0 )
	{
		if( m_pImagePixbuf != NULL )
		{
			// Scale image to window size
			bool bScale = false;
			
			if( m_pImageScaledPixbuf == NULL )
				bScale = true;
			else if(gdk_pixbuf_get_height(m_pImageScaledPixbuf) != m_window.height 
					|| gdk_pixbuf_get_width(m_pImageScaledPixbuf) != m_window.width)
				bScale = true;

			if( bScale )
			{
				if( m_pImageScaledPixbuf != NULL )
					g_object_unref(m_pImageScaledPixbuf);

				#ifdef WEBPNPAPI_DEBUG
					printf("CPlugin::drawWindow() - Scaling to %ix%i\n", m_window.width, m_window.height);
				#endif		
							
				m_pImageScaledPixbuf = gdk_pixbuf_scale_simple( m_pImagePixbuf, m_window.width, m_window.height, GDK_INTERP_BILINEAR );
			}

			// Paint to target area using Cairo
			#ifdef WEBPNPAPI_DEBUG
				printf("CPlugin::drawWindow() - Drawing commenced\n");
			#endif
			
			cairo_t * pCairoContext = gdk_cairo_create(gdkDrawable);

			gdk_cairo_set_source_pixbuf( pCairoContext, m_pImageScaledPixbuf, m_window.x, m_window.y );
			cairo_rectangle( pCairoContext, m_window.x, m_window.y, m_window.width, m_window.height );
			cairo_fill(pCairoContext);

			cairo_destroy(pCairoContext);
			
			
		}
		else
		{
			#ifdef WEBPNPAPI_DEBUG
				printf("CPlugin::drawWindow() - No pixbuf\n");
			#endif
		}		

		pthread_mutex_unlock( &m_mutexImage );
	}
	else
	{
		#ifdef WEBPNPAPI_DEBUG
			printf("CPlugin::drawWindow() - Mutex busy\n");
		#endif	
	}
}

void CPlugin::spawnPopup()
{
	// Popup
	gtk_menu_popup( GTK_MENU(m_gtkMenu) , NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time() );
	gtk_widget_show(m_gtkMenu);
		
	// Done
	return;
}
		
void CPlugin::saveAsPNG( GtkMenuItem * pItem, gpointer pThis )
{
	/* A dirty solution, we copy all the data we need,
	 * in the case that the instance is deleted before we finish here.
	 * Ugly? Very. */
	 
	#ifdef WEBPNPAPI_DEBUG
		printf("CPlugin::saveAsPNG() - Begin\n");
	#endif
		 
	  
	CPlugin * const pInstance = static_cast<CPlugin *>(pThis);
	
	std::string strFilename = "Unnamed";
	std::map<std::string, std::string>::const_iterator itSrc = pInstance->m_mapArgs.find("src");
	
	if( itSrc != pInstance->m_mapArgs.end() )
		strFilename = itSrc->second;
		
	std::string::size_type posSuffix = strFilename.rfind(".");
	if( posSuffix != std::string::npos )
		strFilename.erase( posSuffix );
	strFilename.append(".png");
	
	#ifdef WEBPNPAPI_DEBUG
		printf("CPlugin::saveAsPNG() - Trying to lock mutex.\n");
	#endif
	
	GdkPixbuf * pPixbufCopy = NULL;
	if( pthread_mutex_lock( &pInstance->m_mutexImage ) == 0 )
	{
		#ifdef WEBPNPAPI_DEBUG
			printf("CPlugin::saveAsPNG() - Copying pixbuf\n");
		#endif
	
		if( pInstance->m_pImagePixbuf != NULL )
			pPixbufCopy = gdk_pixbuf_copy(pInstance->m_pImagePixbuf);
				
		pthread_mutex_unlock( &pInstance->m_mutexImage );
	}
	
	if( pPixbufCopy != NULL )
	{ 
		strFilename = saveFileDialog(strFilename);
		if( !strFilename.empty() )
		{
			GError * err = NULL; // Initialize to null
			gdk_pixbuf_save( pPixbufCopy, strFilename.c_str(), "png", &err, NULL); // Save as png and discard errors. We can't do anything.
			//printf("Saving file resulted in the following message: %s", err->message);
		}
		
		g_object_unref(pPixbufCopy);
	}
}

void CPlugin::saveAsWebP( GtkMenuItem * pItem, gpointer pThis )
{
	/* A dirty solution, we copy all the data we need,
	 * in the case that the instance is deleted before we finish here.
	 * Ugly? Very. */
	 
	CPlugin * const pInstance = static_cast<CPlugin *>(pThis);
	
	bool bHasImage = false;
	std::string strStreamCopy;
	
	// Check if instance has a pixbuf
	if( pthread_mutex_lock( &pInstance->m_mutexImage ) == 0 )
	{
		if( pInstance->m_pImagePixbuf != NULL )
			bHasImage = true;
		
		pthread_mutex_unlock( &pInstance->m_mutexImage );
	}
	
	if( bHasImage )
	{
		// Copy stream data
		if( pthread_mutex_lock( &pInstance->m_mutexStream ) == 0 )
		{
			strStreamCopy = pInstance->m_strStreamData;
			pthread_mutex_unlock( &pInstance->m_mutexStream );
		}
		
		// Open dialog
		std::string strFilename = "Unnamed";
		std::map<std::string, std::string>::const_iterator itSrc = pInstance->m_mapArgs.find("src");
		if( itSrc != pInstance->m_mapArgs.end() )
			strFilename = itSrc->second;
		strFilename = saveFileDialog(strFilename);
		
		if( !strFilename.empty() )
		{
			// Output as WebP
			std::ofstream fileOut( strFilename.c_str() );
			if( fileOut.is_open() )
			{
				fileOut.write( strStreamCopy.c_str(), strStreamCopy.size() );
			}
		}
	} 
}

void CPlugin::spawnAbout( GtkMenuItem * pItem, gpointer pData )
{
	const std::string strText = getPluginName() + getPluginDescription() + "\nHomepage: http://code.google.com/p/webp-npapi-linux/\nAuthors: Filip Reesalu, Johan Gustafsson, Jonas Bornold";
	GtkWidget * const pDialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, strText.c_str() );

	gtk_dialog_run( GTK_DIALOG(pDialog) );
	gtk_widget_destroy(pDialog);
}

std::string CPlugin::saveFileDialog(const std::string strFilename)
{
	#ifdef WEBPNPAPI_DEBUG
		printf("CPlugin::saveFileDialog() - Running\n");
	#endif
	
	GtkWidget * gtkDialog;
	gtkDialog = gtk_file_chooser_dialog_new ("Save image",
					      NULL,
					      GTK_FILE_CHOOSER_ACTION_SAVE,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					      NULL);
					      
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (gtkDialog), TRUE);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (gtkDialog), "");
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (gtkDialog), strFilename.c_str() );
	
	std::string strReturnName;
	if( gtk_dialog_run( GTK_DIALOG (gtkDialog) ) == GTK_RESPONSE_ACCEPT )
	{
		char * szFilename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER (gtkDialog) );
		strReturnName = szFilename;
		g_free(szFilename);
		
		#ifdef WEBPNPAPI_DEBUG
			printf("CPlugin::saveFileDialog() - Filename: %s\n", strReturnName.c_str());
		#endif
	}
	gtk_widget_destroy(gtkDialog);
	
	return strReturnName;
}


void CPlugin::streamAsFile( const NPStream * const stream, const std::string strName ) const
{

}

void CPlugin::print(const NPPrint * const platformPrint) const
{

}

void CPlugin::URLNotify(const std::string strURL, const NPReason reason, const void * const notifyData) const
{

}

NPError CPlugin::getValue(const NPPVariable variable, const void * const value) const
{
	return NPERR_GENERIC_ERROR;
}

NPError CPlugin::setValue(const NPNVariable variable, const void * const value) const
{
	return NPERR_GENERIC_ERROR;
}
