/*
 * hfsdump - Inspect the contents of an HFS+ volume
 * Copyright 2013-2017 0x09.net.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// TODO: interpret more fields, output formatting

#include "hfsuser.h"

#include <time.h>
#include <inttypes.h>

#define HFSTIMETOTIMET(x) ((time_t[1]){HFSTIMETOEPOCH(x)})

static inline void dump_volume_header(hfs_volume_header_t vh) {
	char ctimebuf[4][26] = {0};
	printf(
		"volume header:\n"
		"signature: %s\n"
		"version: %" PRIu16 "\n"
		"attributes: hwlock %d unmounted %d badblocks %d nocache %d dirty %d cnids recycled %d journaled %d swlock %d\n"
		"last_mounting_version: %s\n"
		"journal_info_block: %" PRIu32 "\n"
		"date_created: %s"
		"date_modified: %s"
		"date_backedup: %s"
		"date_checked: %s"
		"file_count: %" PRIu32 "\n"
		"folder_count: %" PRIu32 "\n"
		"block_size: %" PRIu32 "\n"
		"total_blocks: %" PRIu32 "\n"
		"free_blocks: %" PRIu32 "\n"
		"next_alloc_block: %" PRIu32 "\n"
		"rsrc_clump_size: %" PRIu32 "\n"
		"data_clump_size: %" PRIu32 "\n"
		"next_cnid: %" PRIu32 "\n"
		"write_count: %" PRIu32 "\n"
		"encodings: %" PRIu64 "\n"
		"finderinfo:\n"
			"\tBoot directory ID: %" PRIu32 "\n"
			"\tStartup parent directory ID: %" PRIu32 "\n"
			"\tDisplay directory ID: %" PRIu32 "\n"
			"\tOS classic system directory ID: %" PRIu32 "\n"
			"\tOS X system directory ID: %" PRIu32 "\n"
			"\tVolume unique ID: %" PRIx64 "\n",
		(char[3]){vh.signature>>8,vh.signature&0xFF,'\0'},vh.version,
		(vh.attributes >> HFS_VOL_HWLOCK)&1, (vh.attributes >> HFS_VOL_UNMOUNTED)&1, (vh.attributes >> HFS_VOL_BADBLOCKS)&1,
		(vh.attributes >> HFS_VOL_NOCACHE)&1, (vh.attributes >> HFS_VOL_DIRTY)&1, (vh.attributes >> HFS_VOL_CNIDS_RECYCLED)&1,
		(vh.attributes >> HFS_VOL_JOURNALED)&1, (vh.attributes >> HFS_VOL_SWLOCK)&1,
		(char[5]){vh.last_mounting_version>>24,(vh.last_mounting_version>>16)&0xFF,(vh.last_mounting_version>>8)&0xFF,vh.last_mounting_version&0xFF,'\0'},
		vh.journal_info_block,
		ctime_r(HFSTIMETOTIMET(vh.date_created),ctimebuf[0]),ctime_r(HFSTIMETOTIMET(vh.date_modified),ctimebuf[1]),
		ctime_r(HFSTIMETOTIMET(vh.date_backedup),ctimebuf[2]),ctime_r(HFSTIMETOTIMET(vh.date_checked),ctimebuf[3]),
		vh.file_count,vh.folder_count,vh.block_size,vh.total_blocks,
		vh.free_blocks,vh.next_alloc_block,vh.rsrc_clump_size,vh.data_clump_size,vh.next_cnid,vh.write_count,
		vh.encodings,
		vh.finder_info[0],vh.finder_info[1],vh.finder_info[2],vh.finder_info[3],vh.finder_info[5],
		(((uint64_t)vh.finder_info[6])<<32)|vh.finder_info[7]
	);
}

static inline void dump_record(hfs_catalog_keyed_record_t rec) {
	char ctimebuf[5][26] = {0};
	hfs_file_record_t file = rec.file; // dump union keys first
	printf(
		"type: %s\n"
		"flags: %" PRIu16 "\n"
		"cnid: %" PRIu32 "\n"
		"date_created: %s"
		"date_content_mod: %s"
		"date_attrib_mod: %s"
		"date_accessed: %s"
		"date_backedup: %s"
		"encoding: %" PRIu32 "\n"
		"permissions.owner_id: %" PRIu32 "\n"
		"permissions.group_id: %" PRIu32 "\n"
		"permissions.admin_flags: %" PRIu8 "\n"
		"permissions.owner_flags: %" PRIu8 "\n"
		"permissions.file_mode: %ho\n"
		"permissions.special: %" PRIu32 "\n",
		(rec.type == HFS_REC_FLDR ? "folder" : "file"),
		file.flags,
		file.cnid,
		ctime_r(HFSTIMETOTIMET(file.date_created),ctimebuf[0]),
		ctime_r(HFSTIMETOTIMET(file.date_content_mod),ctimebuf[1]),
		ctime_r(HFSTIMETOTIMET(file.date_attrib_mod),ctimebuf[2]),
		ctime_r(HFSTIMETOTIMET(file.date_accessed),ctimebuf[3]),
		ctime_r(HFSTIMETOTIMET(file.date_backedup),ctimebuf[4]),
		file.text_encoding,
		file.bsd.owner_id,
		file.bsd.group_id,
		file.bsd.admin_flags,
		file.bsd.owner_flags,
		file.bsd.file_mode,
		file.bsd.special.inode_num
	);
	if(rec.type == HFS_REC_FLDR) {
		hfs_folder_record_t folder = rec.folder;
		printf(
			"valence: %" PRIu32 "\n"
			"user_info.window_bounds: %" PRIu16 ", %" PRIu16 ", %" PRIu16 ", %" PRIu16 "\n"
			"user_info.finder_flags: %" PRIu16 "\n"
			"user_info.location: %" PRIu16 ", %" PRIu16 "\n"
			"finder_info.scroll_position: %" PRIu16 ", %" PRIu16 "\n"
			"finder_info.extended_finder_flags: %" PRIu16 "\n"
			"finder_info.put_away_folder_cnid: %" PRIu32 "\n",
			folder.valence,
			folder.user_info.window_bounds.t, folder.user_info.window_bounds.l, folder.user_info.window_bounds.b, folder.user_info.window_bounds.r,
			folder.user_info.finder_flags,
			folder.user_info.location.v,folder.user_info.location.h,
			folder.finder_info.scroll_position.v,folder.finder_info.scroll_position.h,
			folder.finder_info.extended_finder_flags,
			folder.finder_info.put_away_folder_cnid
		);
	}
	else {
		hfs_file_record_t file = rec.file;
		printf(
			"user_info.file_type: %c%c%c%c\n"
			"user_info.file_creator: %c%c%c%c\n"
			"user_info.finder_flags: %" PRIu16 "\n"
			"user_info.location:  %" PRIu16 ", %" PRIu16 "\n"
			"finder_info.extended_finder_flags: %" PRIu16 "\n"
			"finder_info.put_away_folder_cnid: %" PRIu32 "\n"
			"data_fork.logical_size: %" PRIu64 "\n"
			"rsrc_fork.logical_size: %" PRIu64 "\n",
			(file.user_info.file_type >> 3) & 1,(file.user_info.file_type >> 2) & 1, (file.user_info.file_type >> 1) & 1, file.user_info.file_type & 1,
			(file.user_info.file_creator >> 3) & 1,(file.user_info.file_creator >> 2) & 1, (file.user_info.file_creator >> 1) & 1, file.user_info.file_creator & 1,
			file.user_info.finder_flags,
			file.user_info.location.v,file.user_info.location.h,
			file.finder_info.extended_finder_flags,
			file.finder_info.put_away_folder_cnid,
			file.data_fork.logical_size,
			file.rsrc_fork.logical_size
		);
	}
}

int main(int argc, char* argv[]) {
	if(argc < 2) {
		fprintf(stderr,"Usage: hfsdump <device> [<stat|read> <path|inode>]\n");
		return 0;
	}

	hfs_callbacks cb = {hfs_vprintf, hfs_malloc, hfs_realloc, hfs_free, hfs_open, hfs_close, hfs_read};
	hfslib_init(&cb);
	hfs_volume vol = {0};
	hfs_catalog_keyed_record_t rec; hfs_catalog_key_t key; unsigned char fork;
	int ret = 0;
	if((ret = hfslib_open_volume(argv[1],1,&vol,NULL))) {
		fprintf(stderr,"Couldn't open volume\n");
		hfslib_done();
		return ret;
	}

	if(argc < 4) {
		char name[512];
		hfs_unistr_to_utf8(&vol.name, name);
		printf("Volume name: %s\nJournaled? %d\nReadonly? %d\nOffset: %" PRIu64 "\n",name,vol.journaled, vol.readonly, vol.offset);
		dump_volume_header(vol.vh);
		goto end;
	}

	char* endptr;
	uint32_t cnid = strtoul(argv[3], &endptr, 10);
	if(!*endptr) {
		if((ret = hfslib_find_catalog_record_with_cnid(&vol, cnid, &rec, &key, NULL))) {
			fprintf(stderr,"CNID lookup failure: %" PRIu32 "\n", cnid);
			goto end;
		}
	}
	else if((ret = hfs_lookup(&vol,argv[3],&rec,&key,&fork))) {
		fprintf(stderr,"Path lookup failure: %s\n", argv[3]);
		goto end;
	}

	if(!strcmp(argv[2], "stat")) {
		printf("path: ");
		char* path = hfs_get_path(&vol, rec.folder.cnid);
		if(path)
			printf("%s", path);
		printf("\n");
		free(path);
		dump_record(rec);
	}
	else if(!strcmp(argv[2], "read")) {
		if(rec.type == HFS_REC_FLDR) {
			hfs_catalog_keyed_record_t* keys;
			hfs_unistr255_t* names;
			uint32_t count;
			hfslib_get_directory_contents(&vol,rec.folder.cnid,&keys,&names,&count,NULL);
			for(size_t i = 0; i < count; i++) {
				char name[512];
				hfs_pathname_to_unix(names+i,name);
				puts(name);
			}
			free(names);
			free(keys);
		}
		else if(rec.type == HFS_REC_FILE) {
			hfs_extent_descriptor_t* extents = NULL;
			uint16_t nextents = hfslib_get_file_extents(&vol,rec.file.cnid,fork,&extents,NULL);
			uint64_t bytes = 0, offset = 0, size = fork == HFS_DATAFORK ? rec.file.data_fork.logical_size : rec.file.rsrc_fork.logical_size;
			char data[4096];
			while(!(ret = hfslib_readd_with_extents(&vol,data,&bytes,4096,offset,extents,nextents,NULL)) && offset < size) {
				fwrite(data,(size-offset < bytes ? size-offset : bytes),1,stdout);
				offset += bytes;
			}
			free(extents);
		}
	}
	else fprintf(stderr,"valid commands: stat, read\n");

end:
	hfslib_close_volume(&vol,NULL);
	hfslib_done();
	return ret;
}
