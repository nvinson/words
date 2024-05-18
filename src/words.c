#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_BUF_SIZE 65536

struct entry {
    char *word;
    size_t count;
    struct entry *next;
    struct entry *prev;
};

struct linked_hash_map {
    size_t size;
    size_t cap;
    struct entry *head;
    struct entry *tail;
    struct entry **entries;
};

static int init_map(struct linked_hash_map *map) {
    map->size = 0;
    map->cap = 16;
    map->head = map->tail = NULL;
    map->entries = malloc(16 * sizeof(*map->entries));
    if (map->entries == NULL) {
        return -1;
    }
    for (size_t i = 0; i < map->cap; i++) {
        map->entries[i] = NULL;
    }
    return 0;
}

static size_t gen_hash(char* word) {
    size_t hash = 0;
    char *c = word;
    for (c = word; *c; c++) {
        hash = hash * 31 + *c;
    }
    return hash;
}

static int update_map(struct linked_hash_map *map, char *word) {
    size_t i;
    size_t idx;
    if ((map->cap >> 2) * 3 <= map->size) {
        size_t new_cap = map->cap << 1;
        if (new_cap < map->cap) {
            errno = ENOMEM;
            return -1;
        }
        map->cap = new_cap;
        free(map->entries);
        map->entries = malloc(map->cap * sizeof(*map->entries));
        if (map->entries == NULL) {
            return -2;
        }
        for (i = 0; i < map->cap; i++) {
            map->entries[i] = NULL;
        }
        for (struct entry *l = map->head; l != NULL; l = l->next) {
            for (i = 0, idx = gen_hash(l->word) % map->cap; i < map->cap; idx = (idx + i++) % map->cap) {
                if (map->entries[idx] == NULL) {
                    map->entries[idx] = l;
                    break;
                }
            }
        }
    }
    for (i = 0, idx = gen_hash(word) % map->cap; i < map->cap; idx = (idx + i++) % map->cap) {
        if (map->entries[idx] == NULL) {
            struct entry *e = malloc(sizeof(*e));
            if (e == NULL) {
                return -3;
            }

            e->word = strdup(word);
            if (e->word == NULL) {
                free(e);
                return -4;
            }
            map->size++;
            e->count = 1;
            map->entries[idx] = e;

            if (map->tail != NULL) {
                map->tail->next = e;
                e->prev = map->tail;
                e->next = NULL;
                map->tail = e;
            } else {
                map->head = map->tail = e;
                e->prev = e->next = NULL;
            }
            break;
        }
        if (!strcmp(map->entries[idx]->word, word)) {
            map->entries[idx]->count++;
            struct entry *cur = map->entries[idx];
            while (cur->prev != NULL && cur->prev->count < cur->count) {
                char *tmp_word = cur->word;
                size_t tmp_count = cur->count;

                cur->word = cur->prev->word;
                cur->count = cur->prev->count;
                cur->prev->word = tmp_word;
                cur->prev->count = tmp_count;
                cur = cur->prev;
            }

            break;
        }
    }
    return 0;
}

static void free_map(struct linked_hash_map *map) {
    if (map != NULL) {
        struct entry *l = map->head;
        while(l != NULL) {
            struct entry *t = l;
            l = l-> next;
            free(t->word);
            free(t);
        }
        free(map->entries);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <path to words file>", argv[0]);
        return 1;
    }
    char *buffer;
    ssize_t bytes_read = 0;
    int fd;

    buffer = malloc(MAX_BUF_SIZE * sizeof(*buffer));
    if (buffer == NULL) {
        perror("buffer: malloc");
        return 2;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        free(buffer);
        return 3;
    }
    int in_word = 0;
    size_t word_buf_cap = 16;
    size_t word_buf_sz = 0;
    char *word_buf = malloc(word_buf_cap * sizeof(*word_buf));

    if (word_buf == NULL) {
        perror("word buffer: malloc");
        close(fd);
        free(buffer);
        return 4;
    }

    struct linked_hash_map map;
    if (init_map(&map)) {
        perror("map: init_map");
        close(fd);
        free(buffer);
        return 5;
    }

    while ((bytes_read = read(fd, buffer, MAX_BUF_SIZE)) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            if (in_word) {
                if (!isspace((unsigned char)buffer[i])) {
                    int buf_char = tolower((unsigned char)buffer[i]);
                    if (word_buf_sz + 1 == word_buf_cap) {
                        size_t new_cap = word_buf_cap << 1;
                        if (new_cap < word_buf_cap) {
                            errno = ENOMEM;
                            perror("word buffer");
                            free(word_buf);
                            close(fd);
                            free(buffer);
                            return 6;
                        }
                        char *tmp = realloc(word_buf, new_cap);
                        if (tmp == NULL) {
                            perror("word buffer: realloc");
                            free(word_buf);
                            close(fd);
                            free(buffer);
                            return 7;
                        }
                        word_buf = tmp;
                    }
                    word_buf[word_buf_sz++] = buf_char;
                } else {
                    in_word ^= 1;
                    word_buf[word_buf_sz] = '\0';
                    word_buf_sz = 0;
                    if(update_map(&map, word_buf)) {
                        free_map(&map);
                        free(word_buf);
                        close(fd);
                        free(buffer);
                        free_map(&map);
                        return 8;
                    }
                }
            } else {
                if (!isspace((unsigned char)buffer[i])) {
                    int buf_char = tolower((unsigned char)buffer[i]);
                    word_buf[word_buf_sz++] = buf_char;
                    in_word ^= 1;
                }
            }
        }
    }
    free(word_buf);
    close(fd);
    free(buffer);

    for (struct entry *l = map.head; l; l = l->next) {
        printf("%s %zu\n", l->word, l->count);
    }

    free_map(&map);

    return 0;
}
