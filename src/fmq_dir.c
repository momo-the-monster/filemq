/*  =========================================================================
    fmq_dir - work with file-system directories

    Has some untested and probably incomplete support for Win32.
    -------------------------------------------------------------------------
    Copyright (c) 1991-2012 iMatix Corporation -- http://www.imatix.com
    Copyright other contributors as noted in the AUTHORS file.

    This file is part of FILEMQ, see http://filemq.org.

    This is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the
    Free Software Foundation; either version 3 of the License, or (at your
    option) any later version.

    This software is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTA-
    BILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
    Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see http://www.gnu.org/licenses/.
    =========================================================================*/

/*
@header
    The fmq_dir class works with directories, reading them from disk and
    comparing two directories to see what changed. When there are changes,
    returns a list of "patches".
@discuss
@end
*/

#include <czmq.h>
#include "../include/fmq.h"

//  Structure of our class

struct _fmq_dir_t {
    char *path;             //  Directory name + separator
    zlist_t *files;         //  List of files in directory
    zlist_t *subdirs;       //  List of subdirectories
    time_t time;            //  Most recent file including subdirs
    off_t  size;            //  Total file size including subdirs
    size_t count;           //  Total file count including subdirs
};

#if (defined (WIN32))
static void
s_win32_populate_entry (fmq_dir_t *self, WIN32_FIND_DATA *entry)
{
    if (entry->cFileName [0] == '.')
        ; //  Skip hidden files
    else
    //  If we have a subdirectory, go load that
    if (entry->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        fmq_dir_t *subdir = fmq_dir_new (entry->cFileName, self->path);
        zlist_append (self->subdirs, subdir);
    }
    else {
        //  Add file entry to directory list
        zfile_t *file = zfile_new (self->path, entry->cFileName);
        zlist_append (self->files, file);
    }
}

#else
static void
s_posix_populate_entry (fmq_dir_t *self, struct dirent *entry)
{
    //  Skip . and ..
    if (streq (entry->d_name, ".")
    ||  streq (entry->d_name, ".."))
        return;
        
    char fullpath [1024 + 1];
    snprintf (fullpath, 1024, "%s/%s", self->path, entry->d_name);
    struct stat stat_buf;
    if (stat (fullpath, &stat_buf))
        return;
    
    if (entry->d_name [0] == '.')
        ; //  Skip hidden files
    else
    //  If we have a subdirectory, go load that
    if (stat_buf.st_mode & S_IFDIR) {
        fmq_dir_t *subdir = fmq_dir_new (entry->d_name, self->path);
        zlist_append (self->subdirs, subdir);
    }
    else {
        //  Add file entry to directory list
        zfile_t *file = zfile_new (self->path, entry->d_name);
        zlist_append (self->files, file);
    }
}
#endif


//  --------------------------------------------------------------------------
//  Constructor
//  Loads full directory tree

fmq_dir_t *
fmq_dir_new (const char *path, const char *parent)
{
    fmq_dir_t *self = (fmq_dir_t *) zmalloc (sizeof (fmq_dir_t));
    if (parent) {
        self->path = (char*)malloc (strlen (path) + strlen (parent) + 2);
        sprintf (self->path, "%s/%s", parent, path);
    }
    else
        self->path = strdup (path);
    self->files = zlist_new ();
    self->subdirs = zlist_new ();
    
#if (defined (WIN32))
    //  On Windows, replace backslashes by normal slashes
    char *path_clean_ptr = self->path;
    while (*path_clean_ptr) {
        if (*path_clean_ptr == '\\')
            *path_clean_ptr = '/';
        path_clean_ptr++;
    }
    //  Remove any trailing slash
    if (self->path [strlen (self->path) - 1] == '/')
        self->path [strlen (self->path) - 1] = 0;
    
    //  Win32 wants a wildcard at the end of the path
    char *wildcard = (char*)malloc (strlen (self->path) + 3);
    sprintf (wildcard, "%s/*", self->path);
    WIN32_FIND_DATA entry;
    HANDLE handle = FindFirstFile (wildcard, &entry);
    free (wildcard);
    
    if (handle != INVALID_HANDLE_VALUE) {
        //  We have read an entry, so return those values
        s_win32_populate_entry (self, &entry);
        while (FindNextFile (handle, &entry))
            s_win32_populate_entry (self, &entry);
        FindClose (handle);
    }
#else
    //  Remove any trailing slash
    if (self->path [strlen (self->path) - 1] == '/')
        self->path [strlen (self->path) - 1] = 0;
    
    DIR *handle = opendir (self->path);
    if (handle) {
        //  Calculate system-specific size of dirent block
        int dirent_size = offsetof (struct dirent, d_name)
                        + pathconf (self->path, _PC_NAME_MAX) + 1;
        struct dirent *entry = (struct dirent *) malloc (dirent_size);
        struct dirent *result;

        int rc = readdir_r (handle, entry, &result);
        while (rc == 0 && result != NULL) {
            s_posix_populate_entry (self, entry);
            rc = readdir_r (handle, entry, &result);
        }
        free (entry);
        closedir (handle);
    }
#endif
    else {
        fmq_dir_destroy (&self);
        return NULL;
    }
    //  Update directory signatures
    fmq_dir_t *subdir = (fmq_dir_t *) zlist_first (self->subdirs);
    while (subdir) {
        if (self->time < subdir->time)
            self->time = subdir->time;
        self->size += subdir->size;
        self->count += subdir->count;
        subdir = (fmq_dir_t *) zlist_next (self->subdirs);
    }
    zfile_t *file = (zfile_t *) zlist_first (self->files);
    while (file) {
        if (self->time < zfile_modified (file))
            self->time = zfile_modified (file);
        self->size += zfile_cursize (file);
        self->count += 1;
        file = (zfile_t *) zlist_next (self->files);
    }
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy a directory item

void
fmq_dir_destroy (fmq_dir_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        fmq_dir_t *self = *self_p;
        while (zlist_size (self->subdirs)) {
            fmq_dir_t *subdir = (fmq_dir_t *) zlist_pop (self->subdirs);
            fmq_dir_destroy (&subdir);
        }
        while (zlist_size (self->files)) {
            zfile_t *file = (zfile_t *) zlist_pop (self->files);
            zfile_destroy (&file);
        }
        zlist_destroy (&self->subdirs);
        zlist_destroy (&self->files);
        free (self->path);
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Return directory path

char *
fmq_dir_path (fmq_dir_t *self)
{
    return self->path;
}


//  --------------------------------------------------------------------------
//  Return directory time

time_t
fmq_dir_time (fmq_dir_t *self)
{
    assert (self);
    return self->time;
}


//  --------------------------------------------------------------------------
//  Return directory size

off_t
fmq_dir_size (fmq_dir_t *self)
{
    assert (self);
    return self->size;
}


//  --------------------------------------------------------------------------
//  Return directory count

size_t
fmq_dir_count (fmq_dir_t *self)
{
    assert (self);
    return self->count;
}


//  --------------------------------------------------------------------------
//  Calculate differences between two versions of a directory tree
//  Returns a list of fmq_patch_t patches. Either older or newer may
//  be null, indicating the directory is empty/absent. If alias is set,
//  generates virtual filename (minus path, plus alias).

zlist_t *
fmq_dir_diff (fmq_dir_t *older, fmq_dir_t *newer, char *alias)
{
    zlist_t *patches = zlist_new ();
    zfile_t **old_files = fmq_dir_flatten (older);
    zfile_t **new_files = fmq_dir_flatten (newer);

    int old_index = 0;
    int new_index = 0;

    //  Note that both lists are sorted, so detecting differences
    //  is rather trivial
    while (old_files [old_index] || new_files [new_index]) {
        zfile_t *old = old_files [old_index];
        zfile_t *_new = new_files [new_index];

        int cmp;
        if (!old)
            cmp = 1;        //  Old file was deleted at end of list
        else
        if (!_new)
            cmp = -1;       //  New file was added at end of list
        else
            cmp = strcmp (zfile_filename (old, NULL), zfile_filename (_new, NULL));

        if (cmp > 0) {
            //  New file was created
            if (zfile_is_stable (_new))
                zlist_append (patches, fmq_patch_new (newer->path, _new, patch_create, alias));
            old_index--;
        }
        else
        if (cmp < 0) {
            //  Old file was deleted
            if (zfile_is_stable (old))
                zlist_append (patches, fmq_patch_new (older->path, old, patch_delete, alias));
            new_index--;
        }
        else
        if (cmp == 0 && zfile_is_stable (_new)) {
            if (zfile_is_stable (old)) {
                //  Old file was modified or replaced
                //  Since we don't check file contents, treat as created
                //  Could better do SHA check on file here
                if (zfile_modified (_new) != zfile_modified (old)
                ||  zfile_cursize (_new) != zfile_cursize (old))
                    zlist_append (patches, fmq_patch_new (newer->path, _new, patch_create, alias));
            }
            else
                //  File was created over some period of time
                zlist_append (patches, fmq_patch_new (newer->path, _new, patch_create, alias));
        }
        old_index++;
        new_index++;
    }
    free (old_files);
    free (new_files);
    
    return patches;
}


//  --------------------------------------------------------------------------
//  Return full contents of directory as a patch list. If alias is set,
//  generates virtual filename (minus path, plus alias).

zlist_t *
fmq_dir_resync (fmq_dir_t *self, char *alias)
{
    zlist_t *patches = zlist_new ();
    zfile_t **files = fmq_dir_flatten (self);
    uint index;
    for (index = 0;; index++) {
        zfile_t *file = files [index];
        if (!file)
            break;
        zlist_append (patches, fmq_patch_new (self->path, file, patch_create, alias));
    }
    free (files);
    return patches;
}


//  --------------------------------------------------------------------------
//  Return sorted array of file references

//  Compare two subdirs, true if they need swapping
static bool
s_dir_compare (void *item1, void *item2)
{
    if (strcmp (fmq_dir_path ((fmq_dir_t *) item1),
                fmq_dir_path ((fmq_dir_t *) item2)) > 0)
        return true;
    else
        return false;
}

//  Compare two files, true if they need swapping
//  We sort by ascending name

static bool
s_file_compare (void *item1, void *item2)
{
    if (strcmp (zfile_filename ((zfile_t *) item1, NULL),
                zfile_filename ((zfile_t *) item2, NULL)) > 0)
        return true;
    else
        return false;
}

static int
s_dir_flatten (fmq_dir_t *self, zfile_t **files, int index)
{
    //  First flatten the normal files
    zlist_sort (self->files, s_file_compare);
    zfile_t *file = (zfile_t *) zlist_first (self->files);
    while (file) {
        files [index++] = file;
        file = (zfile_t *) zlist_next (self->files);
    }
    //  Now flatten subdirectories, recursively
    zlist_sort (self->subdirs, s_dir_compare);
    fmq_dir_t *subdir = (fmq_dir_t *) zlist_first (self->subdirs);
    while (subdir) {
        index = s_dir_flatten (subdir, files, index);
        subdir = (fmq_dir_t *) zlist_next (self->subdirs);
    }
    return index;
}


//  --------------------------------------------------------------------------
//  Return sorted array of file references

zfile_t **
fmq_dir_flatten (fmq_dir_t *self)
{
    int flat_size;
    if (self)
        flat_size = self->count + 1;
    else
        flat_size = 1;      //  Just null terminator

    zfile_t **files = (zfile_t **) zmalloc (sizeof (zfile_t *) * flat_size);
    uint index = 0;
    if (self)
        index = s_dir_flatten (self, files, index);
    return files;
}


//  --------------------------------------------------------------------------
//  Remove directory, optionally including all files

void
fmq_dir_remove (fmq_dir_t *self, bool force)
{
    //  If forced, remove all subdirectories and files
    if (force) {
        zfile_t *file = (zfile_t *) zlist_pop (self->files);
        while (file) {
            zfile_remove (file);
            zfile_destroy (&file);
            file = (zfile_t *) zlist_pop (self->files);
        }
        fmq_dir_t *subdir = (fmq_dir_t *) zlist_pop (self->subdirs);
        while (subdir) {
            fmq_dir_remove (subdir, force);
            fmq_dir_destroy (&subdir);
            subdir = (fmq_dir_t *) zlist_pop (self->subdirs);
        }
        self->size = 0;
        self->count = 0;
    }
    //  Remove if empty
    if (zlist_size (self->files) == 0
    &&  zlist_size (self->subdirs) == 0)
        zfile_rmdir (self->path);
}


//  --------------------------------------------------------------------------
//  Print contents of directory

void
fmq_dir_dump (fmq_dir_t *self, int indent)
{
    assert (self);
    
    zfile_t **files = fmq_dir_flatten (self);
    uint index;
    for (index = 0;; index++) {
        zfile_t *file = files [index];
        if (!file)
            break;
        puts (zfile_filename (file, NULL));
    }
    free (files);
}


//  Return file SHA-1 hash as string; caller has to free it
//  TODO: this has to be moved into CZMQ along with SHA1 and hash class

static char *
s_file_hash (zfile_t *file)
{
    if (zfile_input (file) == -1)
        return NULL;            //  Problem reading directory

    //  Now calculate hash for file data, chunk by chunk
    size_t blocksz = 65535;
    off_t offset = 0;
    fmq_hash_t *hash = fmq_hash_new ();
    zchunk_t *chunk = zfile_read (file, blocksz, offset);
    while (zchunk_size (chunk)) {
        fmq_hash_update (hash, zchunk_data (chunk), zchunk_size (chunk));
        zchunk_destroy (&chunk);
        offset += blocksz;
        chunk = zfile_read (file, blocksz, offset);
    }
    zchunk_destroy (&chunk);
    zfile_close (file);

    //  Convert to printable string
    char hex_char [] = "0123456789ABCDEF";
    char *hashstr = (char*)zmalloc (fmq_hash_size (hash) * 2 + 1);
    int byte_nbr;
    for (byte_nbr = 0; byte_nbr < fmq_hash_size (hash); byte_nbr++) {
        hashstr [byte_nbr * 2 + 0] = hex_char [fmq_hash_data (hash) [byte_nbr] >> 4];
        hashstr [byte_nbr * 2 + 1] = hex_char [fmq_hash_data (hash) [byte_nbr] & 15];
    }
    fmq_hash_destroy (&hash);
    return hashstr;
}


//  --------------------------------------------------------------------------
//  Load directory cache; returns a hash table containing the SHA-1 digests
//  of every file in the tree. The cache is saved between runs in .cache.
//  The caller must destroy the hash table when done with it.

zhash_t *
fmq_dir_cache (fmq_dir_t *self)
{
    assert (self);
    
    //  Load any previous cache from disk
    zhash_t *cache = zhash_new ();
    zhash_autofree (cache);
    char *cache_file = (char*)malloc (strlen (self->path) + strlen ("/.cache") + 1);
    sprintf (cache_file, "%s/.cache", self->path);
    zhash_load (cache, cache_file);

    //  Recalculate digest for any new files
    zfile_t **files = fmq_dir_flatten (self);
    uint index;
    for (index = 0;; index++) {
        zfile_t *file = files [index];
        if (!file)
            break;
        char *filename = zfile_filename (file, self->path);
        if (zhash_lookup (cache, zfile_filename (file, self->path)) == NULL)
            zhash_insert (cache, filename, s_file_hash (file));
    }
    free (files);

    //  Save cache to disk for future reference
    zhash_save (cache, cache_file);
    free (cache_file);
    return cache;
}


//  --------------------------------------------------------------------------
//  Self test of this class
int
fmq_dir_test (bool verbose)
{
    printf (" * fmq_dir: ");

    //  @selftest
    fmq_dir_t *older = fmq_dir_new (".", NULL);
    assert (older);
    if (verbose) {
        printf ("\n");
        fmq_dir_dump (older, 0);
    }
    fmq_dir_t *newer = fmq_dir_new ("..", NULL);
    zlist_t *patches = fmq_dir_diff (older, newer, "/");
    assert (patches);
    while (zlist_size (patches)) {
        fmq_patch_t *patch = (fmq_patch_t *) zlist_pop (patches);
        fmq_patch_destroy (&patch);
    }
    zlist_destroy (&patches);
    fmq_dir_destroy (&older);
    fmq_dir_destroy (&newer);

    //  Test directory cache calculation
    fmq_dir_t *here = fmq_dir_new (".", NULL);
    zhash_t *cache = fmq_dir_cache (here);
    fmq_dir_destroy (&here);
    zhash_destroy (&cache);
    
    fmq_dir_t *nosuch = fmq_dir_new ("does-not-exist", NULL);
    assert (nosuch == NULL);
    //  @end

    printf ("OK\n");
    return 0;
}
