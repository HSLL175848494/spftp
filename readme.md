### 构建要求
linux平台
gcc-version >11.1 (需要支持c++20特性)
libevent库已被安装

### DEBUG 版本
```
make debug
```
debug模式构建时将定义_DEBUG宏

### RELEASE 版本
```
make 或 make release
```
### 清理构建文件
```
make clean
```
### 启动服务器
```
./Server
```
默认加载当前目录下的config配置文件
### 指定配置文件路径
```
./Server -config xxxx
```
### 访问服务器
windows：
1.使用ftp命令通过命令行访问
2.在Windows资源管理器中文件路径中输入服务器地址:格式为ftp://服务器地址:端口

linux:
1.使用ftp命令通过命令行访问

### 服务器支持命令
USER - 用户名认证
PASS - 密码认证
QUIT - 断开连接
NOOP - 空操作
SYST - 获取系统类型
FEAT - 查看支持的特性
OPTS - 设置选项（支持 UTF8 切换）
TYPE - 设置传输类型（支持 A/ASCII 和 I/二进制）
PWD - 显示当前工作目录
CWD/XCWD - 改变工作目录
MKD/XMKD - 创建目录
RMD - 删除目录
LIST/NLST - 列出目录内容
RETR - 下载文件
STOR - 上传文件
DELE - 删除文件
SIZE - 获取文件大小
RNFR/RNTO - 文件重命名
PORT - 主动模式设置
PASV - 被动模式设置
OPTS UTF8 ON  - 编码切换