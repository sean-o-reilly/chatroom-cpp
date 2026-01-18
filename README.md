# C++ Chatroom Server using gRPC

```
├── CMakeLists.txt
├── Dockerfile
├── README.md
├── config
│   ├── bot.env
│   ├── client.env
│   ├── client.yaml
│   └── server.env
├── docker-compose.yaml
├── proto
│   └── chatroom.proto
└── src
    ├── bot.cpp
    ├── client.cpp
    └── server.cpp
```


## Build & Run via Docker (Recommended)

##### Requires:
- Linux
- [Docker](https://docs.docker.com/engine/install/ubuntu/)

##### Using Docker Compose:

```
git clone https://github.com/sean-o-reilly/chatroom-cpp.git
cd chatroom-cpp
docker compose up -d server
```

This will build and run the server (it will take a while because the image must build gRPC). 

To connect via a client-side GUI,

```
docker compose run --rm client
CTRL+C to exit the app
```

Use ```docker compose up -d bot``` to test the server's connection with a bot, who just spams "Hello" to the server.

Finally, ```docker compose down``` to teardown the application and all containers.

## Build Locally via CMake (please don't)

##### Requirements:
- Linux
- g++-14 or other C++23 compiler
- gRPC
- protoc

##### Build manually

```
git clone https://github.com/sean-o-reilly/chatroom-cpp.git
cd chatroom-cpp
mkdir build
cmake -S . -B build -DCMAKE_CXX_COMPILER=/usr/bin/g++-14
cmake --build build
```

CMake will expect protoc and gRPC to be installed when building the project.

Included .env files will default to ```localhost:50051```.

#### Run
```./build/Server```
```./build/Client```
```./build/Bot```

## Customization

Edit ```config/client.yaml``` to customize your GUI colors!
