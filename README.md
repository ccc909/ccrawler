# About
This is a polite C++ crawler with an UI made using angular.

![image](https://github.com/ccc909/ccrawler/assets/57506761/c0b49380-2974-4e6a-9a8d-40fa017ae62c)

# Usage

You can either pull the image using 
```
docker pull ccc909/threadedcrawler
```
or build it from the source using
```
git clone https://github.com/ccc909/ccrawler.git
cd ccrawler
docker build -t crawler .
```
and run it using
```
docker run -p 80:80 -p 9001:9001 -d project
```
The C++ crawler might crash and will not restart on its own, the image must be restarted manually if this happens.
