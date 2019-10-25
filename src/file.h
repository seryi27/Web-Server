struct file_data {
    int size;
    void *data;
};

struct file_data *file_load(char *filename);

void file_free(struct file_data *filedata);

