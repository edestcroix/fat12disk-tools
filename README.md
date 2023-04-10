## Building
Calling `make` in the source directory creates the executables
`diskinfo`, `disklist`, `diskget`, and 'diskput`. 
`make clean` removes the build directory and all executables

## diskinfo
`./diskinfo <IMAGE_NAME>.IMA` prints information about the disk

## disklist
`./disklist <IMAGE_NAME>.IMA` lists all files on the disk image.

disklist will list all files and subdirectories in a given directory,
then list the contents of subdirectories in order. It does not
print headers for empty subdirectories, and when listing subdirectory
info it does not print file size or creation time, because subdirectories
do not store this information.

## diskget
`./diskget <IMAGE_NAME>.IMA <FILE>` copies the file out of the disk image into
the current directory. Only works on files in the root directory of the image.

## diskput
`./diskput <IMAGE_NAME>.IMA <DIRECTORY> <FILE>` copies a file from the current directory on the host into the disk image at the given directory.
If no directory path is given, the file will be copied into the root directory.

E.g `./diskput DISK.IMA SUB1/SUB2 FILE.TXT` will put FILE.TXT in SUB1/SUB2, 
and `./diskput DISK.IMA FILE.TXT` will put the file in the root directory.

Diskput sets the creation time in the FAT disk image to the last modified time
of the file on the host system.
