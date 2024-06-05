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
Run the docker image using
```
docker run -p 80:80 -p 9001:9001 -d project
```
