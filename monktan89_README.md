# medooze-media-server
> fork项目https://github.com/medooze/media-server，并采用cmake在MacOS平台编译通过，并未测试性能，为了编译通过修改了少许代码，后面慢慢完善，cmake仅供参考，也希望大家一起完善。

## 准备环境
- 安装brew，cmake等，过程略
- 安装依赖
    ```
    # 1. 安装gtest
    git clone http://github.com/google/googletest
    cd googletest
    mkdir build
    cd build && cmake ..
    make && make install
    
    # 2. 安装其他依赖
    ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)" < /dev/null 2> /dev/null
    
    brew install srtp gsm libgsm xmlrpc-c mp4v2 crc32c
    ```
## 使用
````
# 下载代码
https://github.com/monktan89/medooze-media-server
cd medooze-media-server
# 更新子模块
# 千万不要忘记执行这句命令
git submodule update --init

mkdir build
cd build
# 配置OpenSSL以我本机路径为例
cmake .. -DOPENSSL_ROOT_DIR=/usr/local/Cellar/openssl@1.1/1.1.1h -DOPENSSL_LIBRARIES=/usr/local/Cellar/openssl@1.1/1.1.1h/lib
make
````

