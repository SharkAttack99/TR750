HOW TO  
======  

STEP 1  
------  

You need to have installed gcc, binutils, bzip2, flex, python, perl, make,
find, grep, diff, unzip, gawk, getopt, subversion, libz-dev and libc headers.  

STEP 2  
------  

Install feeds  
```bash  
$ ./scripts/feeds update -a
$ ./scripts/feeds install -a
```  

STEP 3  
------  

Choosing your configuration via `make menuconfig`  
```bash  
Target System --->
	MediaTek Ralink MIPS
	
Subtarget --->
	MT7628 based boards
	
Target Profile --->
	GL-iNet GL-MT750-V2

Global build settings --->
	[*] basic package
	[*] include Luci
	[*] support USB storage and sharing

Kernel modules --->
	I2C support --->
		<*> kmod-i2c-algo-bit
		<*> kmod-i2c-gpio
		<*> kmod-i2c-gpio-custom
		<*> kmod-i2c-mt7628

Other modules  --->
		<*> kmod-bq25890-i2c
		<*> kmod-bq27510-i2c
		<*> kmod-mmc
		<*> kmod-sdhci
		<*> kmod-sdhci-mt7620
		
MTK Properties --->
	Applications --->
		<*> uci2dat
	Drivers --->
		<*> kmod-mt7610e
		<*> kmod-mt7628

Network --->
	< > wpad-mini /* cancel wpad-mini package */
```  

STEP 4  
------  

Preparing files  
```bash  
$ ln -sf package/prepare-files/files files
```  

STEP 5  
------  

Simply running "make" will build your firmware.
It will download all sources, build the cross-compile toolchain, 
the kernel and all choosen applications.
