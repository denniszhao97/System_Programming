/**
 * Finding Filesystems
 * CS 241 - Spring 2019
 */
#include "minixfs.h"
#include "minixfs_utils.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>


#define min(a,b) (((a)<(b))?(a):(b))
/**
 * Virtual paths:
 *  Add your new virtual endpoint to minixfs_virtual_path_names
 */
char *minixfs_virtual_path_names[] = {"info", "myvir"};

/**
 * Forward declaring block_info_string so that we can attach unused on it
 * This prevents a compiler warning if you haven't used it yet.
 *
 * This function generates the info string that the virtual endpoint info should
 * emit when read
 */
static char *block_info_string(ssize_t num_used_blocks) __attribute__((unused));
static char *block_info_string(ssize_t num_used_blocks) {
    char *block_string = NULL;
    ssize_t curr_free_blocks = DATA_NUMBER - num_used_blocks;
    asprintf(&block_string, "Free blocks: %zd\n"
                            "Used blocks: %zd\n",
             curr_free_blocks, num_used_blocks);
    return block_string;
}

// Don't modify this line unless you know what you're doing
int minixfs_virtual_path_count =
    sizeof(minixfs_virtual_path_names) / sizeof(minixfs_virtual_path_names[0]);

int minixfs_chmod(file_system *fs, char *path, int new_permissions) {
    // Thar she blows!
    uint16_t valid = ((uint16_t)new_permissions) & 0x01ff;
    inode* cur = NULL;
    const char* temp = is_virtual_path(path);
    char* virtual = NULL;
    if (temp) {
      virtual = strdup(temp);
      cur = get_inode(fs, virtual);
    } else {
      cur = get_inode(fs, path);
    }

    if (cur == NULL) {
    	errno = ENOENT;
      if(virtual){
        free(virtual);
      }
    	return -1;
    }

    int type = cur->mode >> RWX_BITS_NUMBER;
    cur->mode = (type << RWX_BITS_NUMBER) | valid;
    clock_gettime(CLOCK_REALTIME, &cur->ctim);
    if(virtual){
      free(virtual);
    }
    return 0;
}

int minixfs_chown(file_system *fs, char *path, uid_t owner, gid_t group) {
    // Land ahoy!
    inode* cur = NULL;
    const char* temp = is_virtual_path(path);
    char* virtual = NULL;
    if (temp) {
      virtual = strdup(temp);
      cur = get_inode(fs, virtual);
    } else {
      cur = get_inode(fs, path);
    }

    if (cur == NULL) {
        errno = ENOENT;
        free(virtual);
        return -1;
    }

    if (owner != ((uid_t)-1)) {
	      cur->uid = owner;
    }
    if (group != ((gid_t)-1)) {
	      cur->gid = group;
    }
    clock_gettime(CLOCK_REALTIME, &cur->ctim);
    free(virtual);
    return 0;
}


inode *minixfs_create_inode_for_path(file_system *fs, const char *path) {
    // Land ahoy!
    inode_number first_unused = first_unused_inode(fs);
    if (first_unused == -1 || path == NULL) { // -1 if there are no more inodes in the system
      return NULL;
    }

    const char* filename = (const char*)malloc(MAX_DIR_NAME_LEN);
    inode* parent = parent_directory(fs, path, &filename);
    if (!parent || !valid_filename(filename) || !is_directory(parent)) {
	     return NULL;
    }
    inode* cur = get_inode(fs, path);
    if (cur) { // inode alredy exist
    	clock_gettime(CLOCK_REALTIME, &cur->ctim);
    	return NULL;
    }

    char* this_address = NULL;
    data_block_number block = 0;

    int direct_remain = sizeof(data_block) * NUM_DIRECT_INODES - parent->size;
    if(direct_remain > 0){ // add to direct
      int last_block = parent->size / sizeof(data_block);
      size_t offset  = parent->size % (sizeof(data_block));
      block = add_data_block_to_inode(fs, parent);
      if (block == -1) {
         return NULL;
      }
      this_address = (fs->data_root + parent->direct[last_block])->data + offset;
    } else{ //add to indirect
      inode_number ind = add_single_indirect_block(fs, parent);
      if (ind == -1) {
         return NULL;
      }
      data_block_number* indirect_array = (data_block_number*)(fs->data_root + parent->indirect);
      int total_indirect_size = (-1)*direct_remain;
      int last_indirect_block = total_indirect_size / sizeof(data_block);
      size_t indirect_offset  = total_indirect_size % (sizeof(data_block));
      block = add_data_block_to_indirect_block(fs, indirect_array);
      if (block == -1) {
        return NULL;
      }
      this_address = (fs->data_root + indirect_array[last_indirect_block])->data + indirect_offset;
    }

    if (this_address == NULL) {
      return NULL;
    }

    //Initialize inode
    cur = fs->inode_root + first_unused;
    init_inode(parent, cur);
    minixfs_dirent *entry = malloc(sizeof(minixfs_dirent));
    entry->inode_num = first_unused;
    entry->name = (char*)filename;
    make_string_from_dirent(this_address, *entry);
    parent->size += MAX_DIR_NAME_LEN;
    free(entry);

    return cur;
}

ssize_t minixfs_virtual_read(file_system *fs, const char *path, void *buf,
                             size_t count, off_t *off) {
    // TODO implement the "info" virtual file here
    char **temp = minixfs_virtual_path_names;
    int success = 0;
    while(*temp){
      if(!strcmp(path, *temp)){
        success = 1;
      }
      temp++;
    }

    if (success) {
    	size_t num_used_blocks = 0;
    	int bound = (int)fs->meta->dblock_count;
    	for (int i = 0; i < bound; i++) {
    	    if(get_data_used(fs, i) == 1){
            num_used_blocks++;
          }
    	}

      char* block_string = block_info_string((ssize_t) num_used_blocks);
    	char* file_address = block_string;
    	size_t readsize = 0;
      int space = 0;
    	while (*file_address != '\n' && space) {
        if(*file_address == '\n'){
          space++;
        }
        file_address++;
        readsize++;
      }
    	readsize++;
    	if ((size_t)(*off) >= readsize) {
    	    return 0;
    	}
      count = min(count, (readsize - (size_t)(*off)));
    	char* final_address = block_string + (size_t)(*off);
    	memcpy(buf, final_address, count);
    	*off += count;
    	return (ssize_t)count;
    }

    errno = ENOENT;
    return -1;
}

ssize_t minixfs_write(file_system *fs, const char *path, const void *buf,
                      size_t count, off_t *off) {
    // X marks the spot
    int total_blocks = ((size_t)(*off) + count)/sizeof(data_block);
    size_t last_off = ((size_t)(*off) + count) % sizeof(data_block);
    if (last_off > 0) {
      total_blocks++;
    }
    size_t last = (size_t)(*off)/sizeof(data_block);
    size_t offset = (size_t)(*off) % sizeof(data_block);
    size_t remain = sizeof(data_block) - offset;
    if (minixfs_min_blockcount(fs, path, total_blocks) == -1 ||
       (total_blocks > (int)(NUM_DIRECT_INODES + NUM_INDIRECT_INODES)) ||
       last >= (NUM_DIRECT_INODES + NUM_INDIRECT_INODES)) {
      	errno = ENOSPC;
      	return -1;
    }

    inode* cur = get_inode(fs, path);
    clock_gettime(CLOCK_REALTIME, &cur->mtim);
    clock_gettime(CLOCK_REALTIME, &cur->atim);

    int indirect_start = 0;
    const void* cur_location = buf;
    char* begin = NULL;
    size_t write = 0;
    // find start
    if (last < NUM_DIRECT_INODES) {
      begin = (fs->data_root + cur->direct[last])->data + offset;
    } else {
      indirect_start = 1;
      data_block_number* indirect_array = (data_block_number*)(fs->data_root + cur->indirect);
      begin = (fs->data_root + indirect_array[last - NUM_DIRECT_INODES])->data + offset;
    }

    //clean the first write
    if (count <= remain) {
	      memcpy(begin, buf, count);
	      cur->size = *off + count;
        *off += count;
        return count;
    } else {
      memcpy(begin, buf, remain);
      cur_location += remain;
      write += remain;
    }

    int current_block = last + 1;
    int last_block = (count - remain)/sizeof(data_block) + current_block;
    char* file_address= NULL;
    while(write < count){
      if(indirect_start){ // indirect
        data_block_number* indirect_array = (data_block_number*)(fs->data_root + cur->indirect);
        if(current_block != last_block){
          file_address = (fs->data_root + indirect_array[current_block - NUM_DIRECT_INODES])->data;
          memcpy(file_address, cur_location, sizeof(data_block));
          cur_location += sizeof(data_block);
          write += sizeof(data_block);
        } else { //last one
          file_address = (fs->data_root + indirect_array[last_block - NUM_DIRECT_INODES])->data;
          memcpy(file_address, cur_location, last_off);
          cur_location += last_off;
          write += last_off;
        }
      } else { // direct
        if(current_block != last_block){
          if(current_block < NUM_DIRECT_INODES){ // still direct
            file_address = (fs->data_root + cur->direct[current_block])->data;
            memcpy(file_address, cur_location, sizeof(data_block));
            cur_location += sizeof(data_block);
            write += sizeof(data_block);
          } else { // become indirect
            data_block_number* indirect_array = (data_block_number*)(fs->data_root + cur->indirect);
            file_address = (fs->data_root + indirect_array[current_block - NUM_DIRECT_INODES])->data;
            memcpy(file_address, cur_location, sizeof(data_block));
            cur_location += sizeof(data_block);
            write += sizeof(data_block);
          }
        } else {
          if(current_block < NUM_DIRECT_INODES){ // still direct
            file_address = (fs->data_root + cur->direct[current_block])->data;
            memcpy(file_address, cur_location, last_off);
            cur_location += last_off;
            write += last_off;
          } else { // last one
            data_block_number* indirect_array = (data_block_number*)(fs->data_root + cur->indirect);
            file_address = (fs->data_root + indirect_array[last_block - NUM_DIRECT_INODES])->data;
            memcpy(file_address, cur_location, last_off);
            cur_location += last_off;
            write += last_off;
          }
        }
      }
      current_block++;
    }

    cur->size = (size_t)(*off) + count;
    *off += count;
    return count;
}

ssize_t minixfs_read(file_system *fs, const char *path, void *buf, size_t count,
                     off_t *off) {

    if (path == NULL) {
      errno = ENOENT;
      return -1;
    }
    const char *virtual_path = is_virtual_path(path);
    if (virtual_path){
      return minixfs_virtual_read(fs, virtual_path, buf, count, off);
    }

    inode* cur = get_inode(fs, path);
    if (cur == NULL) {
      errno = ENOENT;
      return -1;
    }

    // updata atim
    clock_gettime(CLOCK_REALTIME, &cur->atim);
    uint64_t myoff = (uint64_t)(*off);
    count = min(count, cur->size - myoff);
    size_t last = myoff/sizeof(data_block); //initial last_block
    if (myoff >= cur->size || last >= (NUM_INDIRECT_INODES +  NUM_DIRECT_INODES)) {
       return 0;
    }

    size_t offset = myoff % sizeof(data_block);
    size_t remain = sizeof(data_block) - offset;
    int indirect_start = 0;
    char* cur_location = buf;
    char* begin = NULL;
    size_t read = 0;
    // find start
    if (last < NUM_DIRECT_INODES) {
    	begin = (fs->data_root + cur->direct[last])->data + offset;
    } else {
    	indirect_start = 1;
    	data_block_number* indirect_array = (data_block_number*)(fs->data_root + cur->indirect);
    	begin = (fs->data_root + indirect_array[last - NUM_DIRECT_INODES])->data + offset;
    }
    //clean the first read

    if (count <= remain) {
	      memcpy((char*)buf, begin, count);
	      cur_location += count;
        read += count;
        *off += count;
        return count;
    } else {
      memcpy((char*)buf, begin, remain);
      cur_location += remain;
      read += remain;
    }

    int current_block = last+1;
    int last_block = (count - remain)/sizeof(data_block) + current_block;
    size_t last_off = (count - remain) % sizeof(data_block);
    char* file_address = NULL;
    while(read < count){
      if(indirect_start){ // indirect
        data_block_number* indirect_array = (data_block_number*)(fs->data_root + cur->indirect);
        if(current_block != last_block){
          file_address = (fs->data_root + indirect_array[current_block - NUM_DIRECT_INODES])->data;
          memcpy(cur_location, file_address, sizeof(data_block));
          cur_location += sizeof(data_block);
          read += sizeof(data_block);
        } else { //last one
          file_address = (fs->data_root + indirect_array[last_block - NUM_DIRECT_INODES])->data;
          memcpy(cur_location, file_address, last_off);
          cur_location += last_off;
          read += last_off;
        }
      } else { // direct
        if(current_block != last_block){
          if(current_block < NUM_DIRECT_INODES){ // still direct
            file_address = (fs->data_root + cur->direct[current_block])->data;
            memcpy(cur_location, file_address, sizeof(data_block));
            cur_location += sizeof(data_block);
            read += sizeof(data_block);
          } else { // become indirect
            data_block_number* indirect_array = (data_block_number*)(fs->data_root + cur->indirect);
            file_address = (fs->data_root + indirect_array[current_block - NUM_DIRECT_INODES])->data;
            memcpy(cur_location, file_address, sizeof(data_block));
            cur_location += sizeof(data_block);
            read += sizeof(data_block);
          }
        } else {
          if(current_block < NUM_DIRECT_INODES){ // still direct
            file_address = (fs->data_root + cur->direct[current_block])->data;
            memcpy(cur_location, file_address, last_off);
            cur_location += last_off;
            read += last_off;
          } else { // last one
            data_block_number* indirect_array = (data_block_number*)(fs->data_root + cur->indirect);
            file_address = (fs->data_root + indirect_array[last_block - NUM_DIRECT_INODES])->data;
            memcpy(cur_location, file_address, last_off);
            cur_location += last_off;
            read += last_off;
          }
        }
      }
      current_block++;
    }

    *off += count;
    return count;
}

