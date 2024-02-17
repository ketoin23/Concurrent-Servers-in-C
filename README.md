# Concurrent Servers in C

Imlementation of concurrent severs in c from scratch using this awesome blog (https://eli.thegreenplace.net/2017/concurrent-servers-part-1-introduction/)  as a tutorial.

## Dependencies
Socket : https://man7.org/linux/man-pages/man2/socket.2.html

libuv : https://github.com/libuv/libuv/tree/v1.x

Event Driven :

epoll : https://man7.org/linux/man-pages/man7/epoll.7.html

select : https://man7.org/linux/man-pages/man2/select.2.html

Concurrent :

pthread : https://man7.org/linux/man-pages/man7/pthreads.7.html

thread-pool : https://github.com/Pithikos/C-Thread-Pool

## How to Run
### Server : 
```
make && ./[name of server to run]
```

### Client :
```
python3 Client.py -n [number of clients] localhost 9090
```
