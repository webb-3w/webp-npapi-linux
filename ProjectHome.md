**Work** **in** **progress**

# Introduction #
A plugin that enables the user to view [WebP](http://code.google.com/speed/webp/) images supplied by the HTML-tags `<object>` and `<embed>`.
The plugin is actively tested on Firefox but should support other NPAPI-compliant browsers on Linux.

# Getting Started #
## Needed Files ##
You will need **libwebp** in order to use the plugin which can be downloaded according to the instructions found [here](http://code.google.com/speed/webp/download.html).
## Installation ##
Save one of the available [binary files](http://code.google.com/p/webp-npapi-linux/downloads/list) to either `/home/username/.mozilla/plugins/` or to install for all users `/usr/lib/mozilla/plugins/` or possibly `/usr/lib/firefox/plugins` depending on your setup.
## Test It! ##
Try viewing [this image](http://www.gstatic.com/webp/gallery/1.sm.webp) or try clicking any of the WebP links available at [Googles WebP gallery](http://code.google.com/speed/webp/gallery.html#contents).

# Extra Files Needed for Compiling #
  * [libwebp](http://code.google.com/speed/webp/download.html)
  * Access to NPAPI for example by downloading the [Gecko SDK](https://developer.mozilla.org/en/gecko_sdk)(we are using version 1.9.2 which gives us support for Firefox 3.6 and onwards).
  * The GTK+ development files.



---

# Future Goals #
We are working on creating our own WebP-decoder.