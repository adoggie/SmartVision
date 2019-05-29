
arm设备需主动下载innerbox程序
拷贝相关运行配置文件到 /home/smartbox

cd smartbox
rm -rf innerbox
wget http://192.168.0.2:8080/smartvision/Branches/innerproc_2.0/build/innerbox2
chmod +x innerbox2
mv innerbox2 innerbox

运行  innerbox
