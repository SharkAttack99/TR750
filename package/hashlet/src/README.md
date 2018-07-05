Cryptotronix Hashlet
=====

[![Build Status](https://travis-ci.org/cryptotronix/hashlet.png)](https://travis-ci.org/cryptotronix/hashlet)
<a href="https://scan.coverity.com/projects/1908">
  <img alt="Coverity Scan Build Status"
       src="https://scan.coverity.com/projects/1908/badge.svg"/>
</a>

Getting
---

The easiest way to install hashlet right now is to add my debian [repository](http://debian.cryptotronix.com/) and then just:

```
sudo apt-get update
sudo apt-get install hashlet
```

However, I've only packaged the binary for ARMv7. If you want to help package a binary for another system, I'd appreciate the help!

The second easiest way to install the software is to download the latest [release](http://download.savannah.gnu.org/releases/hashlet/hashlet-1.1.0.tar.gz) and then:

```
./configure && make
sudo make install
```

If you want to hack on hashlet, read more about building below.

Building
----

This project uses Autotools and you need that installed to configure and build the executable.  I am mainly developing on a BeagleBone Black using Debian.

If you pull this repo (i.e. a non-release), you will need the following dependencies:
- autotools (i.e. automake, autconf, and libtool)
- Flex and Bison
- texinfo (for the documentation if you so desire)

The run time dependencies are:
- libgcrypt

Hardware
---

The hardware was available from [Cryptotronix](http://cryptotronix.com/products/hashlet/).  We are an open hardware company, so see the `hardware` folder for the design to make this yourself. However, this software should work on any board that has an ATSHA204 in the I2C variety.


Running
---

see `./hashlet --help` for full details.  The default I2C bus is `/dev/i2c-1` and this can be changed with the `-b` option.  On some BBB, the bus is `/dev/i2c-2`.  See this [blog post](http://datko.net/2013/11/03/bbb_i2c/) for further details on BBB I2C.

Root
---

You'll need to run as root to access `/dev/i2c*` initially.  You can change this by adding your user to the `i2c` group with:

`sudo usermod -aG i2c user`

Or:

`sudo chmod o+rw /dev/i2c*`


Currently supported commands:

### state
```bash
./hashlet state
Factory
```

This is the first command you should run and verify it's in the Factory state.  This provides the assurance that the device has not been tampered during transit.

### personalize
```bash
./hashlet personalize
```

With the key import feature:

```bash
./hashlet personalize -f keys.txt
```

This is the second command you should run.  On success it will not output anything.  Random keys are loaded into the device and saved to `~/.hashlet` as a backup.  Don't lose that file.  Keys from another hashlet can be imported with the `-f` option, where the file is not also named `~/.hashlet`.

### random
```bash
./hashlet random
62F95589AC76855A8F9204C9C6B8B85F06E6477D17C3888266AEE8E1CBD65319
```

This command also takes the `-B` parameter which allows you to specify the number of bytes you want back from the random number generator.

### mac
```bash
./hashlet mac --file test.txt
mac       : C3466ABB8640B50938B260E17D86489D0EBB3F9C8009024683CB225FFFD3B4E4
challenge : 9F0751C90770E6B40E34BA8E06EFE453FAA46B5FB26925FFBD664FAF951D000A
meta      : 08000000000000000000000000
```

On success it will output three parameters:

1. mac: (aka challenge response) The result of the operation
2. challenge: This is the input to the Hashlet, after a SHA256 digest
3. meta: Meta data that must accompany the result

### check-mac
```bash
./hashlet check-mac -r C3466ABB8640B50938B260E17D86489D0EBB3F9C8009024683CB225FFFD3B4E4 -c 9F0751C90770E6B40E34BA8E06EFE453FAA46B5FB26925FFBD664FAF951D000A -m 08000000000000000000000000
```

Checks the MAC that was produced by the Hashlet.  On success, it will with an exit value of 0.

### offline-verify
```bash
./hashlet offline-verify -c 322B3FFC3BE16B4CC5B445F8E666D0BA5C5E676D00FABD2308AD51243FA0B067 -r FB19B1C63161B6C34CA9D291D1CD16F98247BBA9A298775F795161BEB95BB6EF
```

On success, it will output an exit code of 0, otherwise it will fail.  The point of this command is that a remote server can verify the MAC from the Hashlet without a device.  The keys are written to `~/.hashlet` upon personalization and if this file is store on the server, it can verify a MAC.

The workflow goes like this:

1. Mac some data to produce a challenge response.
2. Send the challenge and MAC to the remote server, which has the key store file.
3. Perform offline-verify on the remote server.

### hmac
```bash
hashlet hmac -f ChangeLog
CD0765AB1F94698E0EACDE22C10F362E925F4F4017B75FDE5AB3124FCEBE9754
```
```bash
echo test | ./hashlet hmac
9CAF53A19F4A8D751F9D03ED3991EF648DC9246D2B329D9A650307212D994326
```

Performs HMAC-256. Your input data is combined with data on the device, so this isn't a pure HMAC-256. Your data will be applied to SHA-256 and then combined, per the datasheet, with device data. That result is fed into HMAC-256.

### offline-hmac
```bash
hashlet offline-hmac -r CD0765AB1F94698E0EACDE22C10F362E925F4F4017B75FDE5AB3124FCEBE9754 -f ChangeLog
echo $?
0
```
```bash
echo test | ./hashlet offline-hmac -r 9CAF53A19F4A8D751F9D03ED3991EF648DC9246D2B329D9A650307212D994326
echo $?
0
```

Similar to `offline-verify`, verifies the hmac operation using the key file from `~/.hashlet`.
### serial-num
```bash
./hashlet serial-num
0123XXXXXXXXXXXXEE
```
X's indicate the unique serial number.

Options
---

Options are listed in the `--help` command, but a useful one, if there are issues, is the `-v` option.  This will dump all the data that
travels across the I2C bus with the device.


Design
---

In the `hardware` folder, one should find the design files for the Hashlet.  The IC on the hashlet is the [Atmel ATSHA204](http://www.atmel.com/Images/Atmel-8740-CryptoAuth-ATSHA204-Datasheet.pdf).

Support
---

IRC: Join the `#cryptotronix` channel on freenode.


Contributing
---
Pull requests welcome :)
