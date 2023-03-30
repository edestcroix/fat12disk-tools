/* TODO: Write this file to put files in the disk
 * Will need to write to the FAT table, and update the directory.
 *
 *
 * Updating the FAT table is going to be annoying, because it
 * uses 12 bit sections in little-endian, so making sure
 * not to overwrite any other data is going to be a pain.
 * for the weird bytes with bits of 2 values in it, going to
 * have to read it out, AND out the bits to change, OR
 * in the new ones, and write it back. Also: should
 * these modifications be done to the fat_table buffer, then
 * the whole thing written back to the disk? Or should
 * the values be written directly to the disk?
 *
 *
 * General Idea:
 * - check for free space.
 * - Navigate to the location in the filesystem to copy to
 * - add a dir entry for the file to the directory,
 * - find a free cluster in the FAT table,
 * - read the file into the cluster, find the next free cluster, and repeat
 *   until the file is copied. Do this recursivly, returning the index of the
 *   cluster read to each time, so the previous cluster can be updated in the
 *   FAT table.
 * - Hope it works lol.
 * */
