# serv.h
Single Header Web Server Library

I initially wrote this for a Linux app I'm working on, but I think it might be useful for a few people.
This is written as an alternative to libraries such as electron, webview, and tauri. Instead of hooking to
a browser process, or packing an entire browser in the binary, this lib just starts a HTTP server, and allows
the user to open the link in their favorite browser.
Please note, this is not for every piece of software, choose what works best for you.

### Future plans
- [x] Support Linux
- [ ] Support Windows
- [ ] Parse POST requests
- [ ] A better solution for 'bindings'
