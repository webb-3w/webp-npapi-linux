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

#ifndef H_CPLUGIN
#define H_CPLUGIN

// Includes for NPAPI
#include "../../include/npapi.h"
#include "../../include/npfunctions.h"

// Includes
#include <pthread.h>
#include <string>
#include <map>

// Include for pixbuf
#include <gdk/gdk.h>
#include <gtk/gtk.h>

class CPlugin
{
	public: // Functions
		CPlugin( const NPP instance, const NPMIMEType mimeType, const uint16_t mode, const std::map<std::string, std::string> mapArgs, const NPSavedData * const saved );
		~CPlugin();
		
		static void setBrowserFunctions( NPNetscapeFuncs * const pBrowserFunctions );
		
		static const std::string & getPluginName();
		static const std::string & getPluginDescription();
		static const std::string & getPluginVersion();
		
		NPError setWindow(const NPWindow * const window);

		NPError newStream(const NPMIMEType mimeType, const NPStream * const stream, const NPBool seekable, uint16_t * const stype);
		NPError destroyStream(const NPStream * const stream, const NPReason reason);

		int32_t writeReady(const NPStream * const stream);
		int32_t write(const NPStream * const stream, const int32_t offset, const int32_t len, const void * const buffer);
		
		int16_t handleEvent(const void * const event);
		
		void streamAsFile( const NPStream * const stream, const std::string strName) const;
		void print(const NPPrint * const platformPrint) const;
		void URLNotify( const std::string strURL, const NPReason reason, const void * const notifyData) const;
		NPError getValue(const NPPVariable variable, const void * const value) const;
		NPError setValue(const NPNVariable variable, const void * const value) const;
	
	private: // Functions
		void drawWindow( GdkDrawable * const gdkDrawable );
		
		void spawnPopup();
		
		/* These are connected to signals for menu-item activation */
		static void saveAsPNG( GtkMenuItem * pItem, gpointer pThis );
		static void saveAsWebP( GtkMenuItem * pItem, gpointer pThis );
		static void spawnAbout( GtkMenuItem * pItem, gpointer pData );
		
		static std::string saveFileDialog(const std::string strFilename);
						
	private: // Variables
		static NPNetscapeFuncs * s_pBrowserFunctions;
		
		/* Plugin properties */
		static const std::string s_strPluginName;
		static const std::string s_strPluginDescription;
		static const std::string s_strPluginVersion;
		
		/* Instance properties */
		const bool m_bHasSize;
		const bool m_bEmbedded;
		std::map<std::string, std::string> m_mapArgs;
	
		NPP m_npp;
		NPWindow m_window;
	
		/* Stream variables for image */
		pthread_mutex_t m_mutexStream;
		const NPStream * m_pStream;
		std::string m_strStreamData;
		
		/* Pixbuf wrappers for image data */
		pthread_mutex_t m_mutexImage;
		uint8_t * m_pImageRawData;
		GdkPixbuf * m_pImagePixbuf;
		GdkPixbuf * m_pImageScaledPixbuf;
		
		/* Temporary */
		GtkWidget * m_gtkMenu;	
};

#endif