# How to expand Veracrypt system partition WITHOUT decrypting

You say it cannot be done, well I say it can be done! This guide assumes the partition you will expand (C:) is at the end of the disk and is the last partition. if you need to shuffle shit around, see the bottom of the guide for more info. It also assumes you are on a modern system: Windows 10, UEFI and GPT. Also all linux commands are run as root, use `sudo su` to get a root shell.

**THIS GUIDE IS FOR EXPERTS ONLY.** it's probably good to skim over the whole guide before you start messing around. while you are performing these steps, you should be like a surgeon: extremely methodical, careful, and precise. however, unlike surgeons in real life, we have the luxury to make backups whenever we want. so it's also a good idea to make a backup of the whole disk before beginning.

# if your data partition is the last partition and there is free space directly next to it on the right

**Synopsis**: (1) rewrite veracrypt volume header with new size (2) expand partition and ntfs in windows

Prerequisite:
 - build this repo from source for Linux. No GUI needed, only CLI build is necessary.
 - ubuntu livecd

0. compile the patched veracrypt from this repo. we will use it to patch the veracrypt volume header later to update the field that specifies the volume size and length. (you can use [static.py](https://github.com/stong/static.py) to make a redistributable version that can run on livecd without having to compile on the livecd.)

1. UNPLUG all the other drives from your computer. Plug them back in only at the end when you are confident everything is working.
2. boot into ubuntu
3. lsblk and see which is the partition you want to expand. also, check what DISK this is on. For example my disk is /dev/sda and the partition is /dev/sda3.
3. make your other partitions readonly to limit the possibility of you fucking up. do this with `blockdev --setro /dev/sda1` for example
4. view, and backup the veracrypt header, just in case. Veracrypt keeps a 512 byte encrypted header for the whole disk at offset 7C00 (sector 62). you can view it with dd

```
dd if=/dev/sda bs=512 skip=62 count=1 | xxd
```

NOTE, this is on /dev/sda, not any partition. This header is placed before any of the partition data!

And you should copy your backup to a flash drive or something.

5. now we can expand dong the partition. run `gdisk /dev/sda`. we are just using it to view the partition table. we won't actually write any changes to our partition table.

6. list the partition table with `p` and for each one, do `i` on it and take a picture of this info with your phone as a backup.

7. so far we assumed your last partition is the one holding your veracrypt C:\, that you wish to expand. delete this partition with `d`.

8. then create a new partition with `n` that fills the entire rest of the disk. or whatever desired size you want to have. then do `i` on it and write down how many SECTORS **size** it is.

9. Then use Ctrl-C to exit `gdisk`. **DON'T WRITE CHANGES.**

10. multiply the size you got in **SECTORS** by 512 to get size in **BYTES**.

11. now use the janky veracrypt expander I wrote. do `VERACRYPT_NEW_SIZE=<new size in bytes> ./veracrypt -m ro,system --filesystem=none /dev/sda3` and it will calculate a new volume header (but it won't write anything to disk.) obviously, you will need to replace `/dev/sda3` with your partition name

12. Backup the output of this file. **But you should probably exclude the decrypted header dump as this contains sensitive info, your volume's master encryption key.**

13. the last chunk of output, is a hex dump of 512 bytes of the new header that has the desired size you want. save this hex dump (JUST THE HEX DUMP) to a file, let's say `newheader.txt`.

14. convert the hex dump into a bin file `xxd -r -p < newheader.txt > newheader.bin`

15. write this header to disk. **this is a destructive operation but we should have backed everything up already at this point if you followed the instructions.** do `dd if=newheader.bin of=/dev/sda bs=512 seek=62 count=1 conv=notrunc,sync`. **NOTE the destination is the DRIVE device sda, NOT the PARTITION sda3 or sda1.**

16. you can run the `./veracrypt` command again to verify that the header is properly written and the size field is updated.

17. at this point, probably a good idea to mount your C: drive just to make sure things are still in order. `mkdir /mnt/c ; mount -o ro /dev/mapper/veracrypt1 /mnt/c`

17. Now you're pretty much good to go. Reboot into veracrypt bootloader. AT THE VERACRYPT PASSWORD PROMPT. when you put in your password. it should say something like `Success start 0 12341234 length 5432154321`. The length, should be the NEW SIZE that you patched the header with.

18. when windows boots go to disk management, you can just extend the partition as necessary. be careful not to overextend past how much you extended veracrypt to.

**CAVEAT**: these steps won't randomize the expanded region on the disk. this could lead to attacker being able to tell free blocks from used ones. if you are concerned about this, you can fill the disk with /dev/urandom first

# what if your data partition is not the last partition.

**Synopsis**: (1) copy all data to secondary temp disk (2) repartition disk (3) reinstall windows (temporarily) and veracrypt (4) rewrite veracrypt volume header with new size (5) copy contents back from within encrypted container to within encrypted container (6) expand partition and ntfs in windows

unforunately, you are going to have to shuffle shit around. you need:

 - windows install media usb (liveusb)
 - another disk that has equivalent size or later

0. physically unplug all other disks from the computer.

1. dd the entire source drive to the temporary drive. `dd if=/dev/sda of=/dev/sdb bs=1M conv=sync status=proress iflag=fullblock`

2. go into `gdisk /dev/sda` and do `i` on every partition and SAVE THE INFO for later. also, back up the veracrypt volume header at sector 62.

3. delete all the partitions on the disk. (at this point, if you're using an ssd, you can TRIM the whole disk if you want.) then recreate the partitions as desired. ideally EFI system partition can come first, then Microsoft reserved, then C:\data partition.

4. for each partition other than the main data partition, ensure the new partition's size is identical to the original. also make sure ith as the same GUID.

5. for each partition, use `x` to go into "expert mode" and then use `c` to change the disk's UUID to match the original disk too.

6. write the partition table to disk. **this will wipe out your windows installation and all the other files on the disk.**

7. now our partitions are setup the way we would like. but how do we get veracrypt to boot this again? and plus we moved around our EFI system partition. i don't know wtf is wrong with windows but, i found the best way is to just reinstall windows onto your new partition table. now it should find the ESP in partition 1, MSR in part 2, and windows in part 3.

8. Then go reinstall veracrypt on windows and encrypt the system drive. if you're impatient you can shrink the ntfs before installing veracrypt so it encrypts less. Also, back up your new rescue disk as the volume master key has changed since we reinstalled veracrypt.

9. Now boot into the linux liveusb.

10. at this step you might as well make your backup disk and all its partitions readonly to prevent any fuckups.

```
blockdev --setro /dev/sdb
blockdev --setro /dev/sdbp1
blockdev --setro /dev/sdbp2
blockdev --setro /dev/sdbp3
```

10. go update the veracrypt volume header in the first section of this guide. DON'T REBOOT INTO WINDOWS YET.

10. ZZRGGGGGGGGRDDDDDDDDDDDDDDDDD  <-- I fell asleep while typing this. ignore

10. use `gdisk` to delete and recreate the data partition so it EXACTLY MATCHES the size in sectors, partition type GUID, and unique identifier UUID, of the original backup data partition. example commands:

```
$ gdisk /dev/sdb
> p
> i
> 2 <-- note down all this information
> Ctrl-C <-- exit gdisk
$ gdisk /dev/sda
> p
> i
> 3
> d
> 3 <-- delete shrunken partition
> n
> ... fill in the relevant info. for the size do +XXXXX where XXXXX is whatever is the size in sectors you got from inspecting /dev/sdb2. and the GUID should probably be the one you got from inspecting that means microsoft basic data partition
> x <-- enter expert mode
> c
> 3 <-- change guid for partition 3
> ... fill in whatever is the GUID you got from inspecting /dev/sdb2
> w <-- write changes to disk
> Ctrl-C <-- gdisk
```

11. check changes with `lsblk` and make sure everything is correct

11. We cannot directly copy the encrypted data from the backup partition to our new expanded partition. This is because veracrypt uses XTS, with absolute sector number as the tweak it seems. So if you directly copy the data to a different location on disk, it will decrypt to garbage. So we must copy the inner contents of the volumes to eachother. Now mount both the unlock veracrypt and the new veracrypt system partitions in linux:

```
veracrypt -m ro,system --filesystem=none /dev/sdb2 # source, this one is our original data backup
veracrypt -m system --filesystem=none /dev/sda3 # destination, this one is the expanded partition
```

(`--filesystem=none` unlocks the volume as block device without mounting a filesystem.)

12. check `lsblk` and see which devmapper (dm) device it is. it will probably be `veracrypt1` and `veracrypt2`. **But the ordering of which one is the src and which one is the dst is nondeterministic. So you need to check and USE THE RIGHT ONES FOR YOUR SYSTEM. DON'T just copy paste the commands or else you will have some nice data loss**

13. make the source volume readonly to prevent any fuck-ups due to fat-fingering the command. `blockdev --setro /dev/mapper/veracryptX` (where `X` is the number for your source dm block device)

14. copy the INNER, unencrypted contents between the two encrypted volumes. we are copying directly from 1 veracrypt container to another. No unencrypted data ever hits the disk, only ram. `dd if=/dev/mapper/veracryptX of=/dev/mapper/veracryptY bs=1M conv=sync status=proress iflag=fullblock`

15. now you should have your old ntfs in the new, expanded veracrypt container. let's mount this to make sure everything is OK. `mkdir /mnt/c; mount -o ro /dev/mapper/veracryptY /mnt/c`. Then go around /mnt/c/ and `ls` and `cat` a few files to make sure shit is still good. if not, you fucked up somewhere along the way and you should start over. but since we have a backup, there should be no chance of data loss.

16. reboot into windows. make sure to boot into the veracrypt bootloader that gives you the password prompt. at the prompt, when it says `Success` make sure the new length is that you patched the volume header with.

17. go to disk management and resize the ntfs partition. DON'T make the size bigger than the veracrypt volume header length or you risk corrupting your filesystem. For me i leave a few MB unallocated at the end of the disk as buffer just in case.

18. hooray, now you should be working. reboot to make sure things are working

# troubleshooting

1. if you can't get to veracrypt bootloader. for example it dumps you directly into windows boot manager (which fails). Go to your motherboard UEFI settings and fix the boot order

2. you can get to veracrypt bootloader but it doesn't chainload windows properly. this is probably a problem with your partition table (wrong GUIDs?) or the veracrypt volume header. that's why I told you to make backup. the veracrypt bootloader is annoying and finnicky. worst case, just reinstall windows and veracrypt and follow the steps to copy your ntfs filesystem.

3. if you can get past veracrypt into windows boot manager, but it does not get to winload (errors with BCD, or blue screen on boot). this means windows boot manager is not happy. there are 3 possibilities: (1) windows doesn't see the unlocked volume provided by veracrypt bootloader, (2) NTFS is corrupt, (3) bcd is invalid. go into recovery, get into command prompt.

4. if C:\ isn't getting mounted properly -- for example, boot fails to BCD error with black background. windows boot manager isn't seeing the system drive provided by veracrypt. check all partition GUIDs. also check your ntfs, trying mounting in ubuntu to see if it is valid. **just because it mounts doesn't mean it's not corrupt. go look through a bunch of files in the filesystem and see if you can properly access them too.**

5. if C:\ is mounted by veracrypt properly. **just because the filesystem mounts doesn't mean it's not corrupt. go look through a bunch of files in the filesystem and see if you can properly access them too.** if there's no ntfs corruption then the issue is probably limited to the BCD. you can probably rebuild the bcd and boot.

```
dir C:\Windows <-- make sure C:\ is properly mounted at this point! if not, something is wrong and you need to start over.
mountvol b: /s   <-- mount ESP to B:
b: <-- change to b drive
cd EFI
cd Microsoft
```

**Option 1: Rebuild all windows bootloader files**

```
move Boot _Boot.bak
rmdir /s /q Boot
mkdir Boot
bcdboot C:\Windows /s b: /v <-- rebuild all boot files
cd Boot
move bootmgfw.efi bootmgfw_ms.vc <-- veracrypt chainloads into the original windows boot manager using this filename
copy B:\Veracrypt\DcsBoot.efi bootmgfw.efi <-- replace windows boot manager with veracrypt bootloader. (this is what the veracrypt installer does)
```

**Option 2: Rebuild BCD only**

```
cd Boot
del BCD
bootrec /rebuildbcd
move bootmgfw.efi bootmgfw_ms.vc <-- veracrypt chainloads into the original windows boot manager using this filename
copy B:\Veracrypt\DcsBoot.efi bootmgfw.efi <-- replace windows boot manager with veracrypt bootloader. (this is what the veracrypt installer does)
```

6. if you get error C0000001 bluescreen on boot, this means your ntfs is corrupted and you probably fucked up while dding it from your backup. or you fucked up the partition sizes. or the veracrypt header is not updated correctly

### troubleshooting checklist

- veracrypt volume header valid?
- partition table valid?
  - EFI system partition is bootable?
  - all partitions have the same GUID as when we started?
- veracrypt reports correct size on password prompt success?
- veracrypt volume contains valid NTFS?
- NTFS isn't corrupt? and you can access all the files too?
- C:\ is mounted in windows recovery?
- rebuilt BCD?


# appendix: how to build this tool on ubuntu

```
apt update
apt install -y build-essential yasm
apt install libwxgtk3.0-gtk3-dev # if ubuntu 20.04
apt install libwxgtk3.0-dev # if ubuntu 18.04
git clone https://github.com/stong/VeraCrypt
cd VeraCrypt
cd src
make NOGUI=1 DEBUG=1 -j `nproc` # output to Main/veracrypt
```
