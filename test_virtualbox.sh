#bin/bash
# install script
#
# 20200615 init
#
# ToDo:
# maak dev lid van sudo


sudo apt-get update --yes

# download en install git
sudo apt-get install git --yes

git config --global user.name "test_dev qelectrotech"
git config --global user.email test_dev@qelectrotech.com

# voor de dev doc van qelectrotech
sudo apt-get install doxygen --yes

# download en install QT

sudo apt-get install build-essential --yes
sudo apt-get install cmake --yes
sudo apt-get install qtcreator --yes
sudo apt-get install qt5-default --yes

cd ~
wget http://download.qt.io/official_releases/online_installers/qt-unified-linux-x64-online.run
sudo chmod +x qt-unified-linux-x64-online.run
./qt-unified-linux-x64-online.run

# download en install de kcoreaddons
sudo apt-get install perl --yes
sudo apt-get install libyaml-libyaml-perl --yes
sudo apt-get install dialog --yes

# add for karchive
sudo apt-get install libbz2-dev --yes
sudo apt-get install liblzma-dev --yes
sudo apt-get install libghc-zlib-dev --yes

mkdir -p ~/kde/src
cd ~/kde/src/
git clone https://invent.kde.org/sdk/kdesrc-build.git

cd ~/kde/src/kdesrc-build

./kdesrc-build-setup
./kdesrc-build kwidgetsaddons karchive kcoreaddons --include-dependencies
cp ~/kde/mkspecs/*.pri /usr/lib/x86_64-linux-gnu/Qt5/mkspecs/modules/

#!
#! faalt op karchive
#! kcoreaddons
#! kwidgetsaddons
#! didn't build


# git qelectrotech
cd ~
git clone git://git.tuxfamily.org/gitroot/qet/qet.git qet_git

cd qet_git

git checkout master

# maak qelectrotech dev doc
doxygen Doxyfile

#! werkt niet

#start qt
cd ~
qtcreator ~/qet_git/qelectrotech.pro
