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

#include "webp-npapi.h"

// Includes
#include <stdexcept>
#include <map>

#include <cstdio>
#include "CPlugin.h"

// These should be moved into the class as static variables retrieved by
// static methods... I'm guessing.
//#define PLUGIN_NAME        "webp-npapi"
//#define PLUGIN_DESCRIPTION PLUGIN_NAME " (Image viewer for WebP)"
//#define PLUGIN_VERSION     "1.0.0.0"
static char * const szVersion = "1.0.0.0"; 

// Called when plugin is loaded by browser
NP_EXPORT(NPError) NP_Initialize(NPNetscapeFuncs * bFuncs, NPPluginFuncs * pFuncs)
{
	// Set browser functions for plugin
	CPlugin::setBrowserFunctions(bFuncs);
	
	// Check the size of the provided structure based on the offset of the last member we need
	if( pFuncs->size < ( offsetof(NPPluginFuncs, setvalue) + sizeof(void*) ) )
		return NPERR_INVALID_FUNCTABLE_ERROR;

	pFuncs->newp = NPP_New;
	pFuncs->destroy = NPP_Destroy;
	pFuncs->setwindow = NPP_SetWindow;
	pFuncs->newstream = NPP_NewStream;
	pFuncs->destroystream = NPP_DestroyStream;
	pFuncs->asfile = NPP_StreamAsFile;
	pFuncs->writeready = NPP_WriteReady;
	pFuncs->write = NPP_Write;
	pFuncs->print = NPP_Print;
	pFuncs->event = NPP_HandleEvent;
	pFuncs->urlnotify = NPP_URLNotify;
	pFuncs->getvalue = NPP_GetValue;
	pFuncs->setvalue = NPP_SetValue;

	return NPERR_NO_ERROR;
}

NP_EXPORT(char*) NP_GetPluginVersion()
{
	return szVersion;
}

NP_EXPORT(char*) NP_GetMIMEDescription()
{
	return (char*) "image/webp:webp:WebP image viewer";
}

NP_EXPORT(NPError) NP_GetValue(void* future, NPPVariable aVariable, void* aValue)
{
	switch (aVariable)
	{
		case NPPVpluginNameString:
			*((const char **) aValue) = CPlugin::getPluginName().c_str();
		break;

		case NPPVpluginDescriptionString:
			*((const char**)aValue) = CPlugin::getPluginDescription().c_str();
		break;

		default:
			return NPERR_INVALID_PARAM;
		break;
	}
	
	return NPERR_NO_ERROR;
}

NP_EXPORT(NPError) NP_Shutdown()
{
	return NPERR_NO_ERROR;
}

// Called for each new instance of the plugin
NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved)
{
	try
	{
		// Add try/catch for memory-allocation error
		std::map<std::string, std::string> mapArgs;
		for( int16_t i = 0; i < argc; ++i )
			mapArgs[ argn[i] ] = argv[i];
		
		// Create plugin instance and save in instance-data		
		CPlugin * pPlugin = new CPlugin( instance, pluginType, mode, mapArgs, saved );
		instance->pdata = pPlugin;
	}
	catch( std::runtime_error & exception )
	{
		return NPERR_GENERIC_ERROR;
	}

	return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData * * save)
{
	CPlugin * pPlugin = static_cast<CPlugin *>(instance->pdata);
	delete pPlugin;

	return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow * window)
{
	CPlugin * pPlugin = static_cast<CPlugin *>(instance->pdata);
	return pPlugin->setWindow(window);
}

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype)
{
	CPlugin * pPlugin = static_cast<CPlugin *>(instance->pdata);
	return pPlugin->newStream( type, stream, seekable, stype );
}

NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
	CPlugin * pPlugin = static_cast<CPlugin *>(instance->pdata);
	return pPlugin->destroyStream( stream, reason );
}

int32_t NPP_WriteReady(NPP instance, NPStream* stream)
{
	CPlugin * pPlugin = static_cast<CPlugin *>(instance->pdata);
	return pPlugin->writeReady( stream );
}

int32_t NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer)
{
	CPlugin * pPlugin = static_cast<CPlugin *>(instance->pdata);
	return pPlugin->write( stream, offset, len, buffer );
}

void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
	CPlugin * pPlugin = static_cast<CPlugin *>(instance->pdata);
	return pPlugin->streamAsFile( stream, fname );
}

void NPP_Print(NPP instance, NPPrint* platformPrint)
{
	CPlugin * pPlugin = static_cast<CPlugin *>(instance->pdata);
	return pPlugin->print( platformPrint );
}

int16_t NPP_HandleEvent(NPP instance, void* event)
{
	CPlugin * pPlugin = static_cast<CPlugin *>(instance->pdata);
	return pPlugin->handleEvent(event);
}

void NPP_URLNotify(NPP instance, const char* URL, NPReason reason, void* notifyData)
{
	CPlugin * pPlugin = static_cast<CPlugin *>(instance->pdata);
	return pPlugin->URLNotify( URL, reason, notifyData );
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
	CPlugin * pPlugin = static_cast<CPlugin *>(instance->pdata);
	return pPlugin->getValue( variable, value );
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
	CPlugin * pPlugin = static_cast<CPlugin *>(instance->pdata);
	return pPlugin->setValue( variable, value );
}
 