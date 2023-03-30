# FAT12disk-tools
Basic tools for interacting with FAT12 formatted disk images.

## Building
Calling `make` in the source directory creates the executables
`diskinfo`, `disklist`, and `diskget`.  
`make clean` removes the build directory and all executables


## diskinfo
`./diskinfo <IMAGE_NAME>.IMA` prints information about the disk

## disklist
`./disklist <IMAGE_NAME>.IMA` lists all files on the disk image.

## diskget
`./diskget <IMAGE_NAME>.IMA <FILE>` copies the file out of the disk image into
the current directory. Only works on files in the root directory of the image.
