#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

struct thread_args {
    void* word_mutex; // used to update global word count
    void* file_mutex; // used to protect position in file from being moved while reading
    int file_descriptor; // used to access file
    int start; // where the chunk starts
    int length; // length of chunk
    int last_chunk; // is this the last chunk
    char prev_char; // used to ensure proper count for words that span chunk boundaries
};

int global_words = 0;
void* reader_thread(void* args){
    struct thread_args* my_args = (struct thread_args*)args;
    void* word_mutex = my_args->word_mutex;
    void* file_mutex = my_args->file_mutex;
    int fd = my_args->file_descriptor;
    int start = my_args->start; 
    int length = my_args->length;
    int last_chunk = my_args->last_chunk;
    char prev_char = my_args->prev_char;
    
    int words = 0;
    int in_word = isgraph(prev_char);
    
    for (int i = 0; i < length; i++){
        char ch;
        pthread_mutex_lock(file_mutex);
            lseek(fd, start+i, SEEK_SET);
            read(fd, &ch, 1); // read character-by-character 
        pthread_mutex_unlock(file_mutex);
        
        // count word when you see the space after it
        if (in_word && isspace(ch)){ 
            words++;
            in_word = 0; 
        }
        else if (!in_word && isgraph(ch)){
            in_word = 1;
        }
    }
    // if file ends in a word, count the word
    if (in_word && last_chunk){
        words++;
    } 
    pthread_mutex_lock(word_mutex);
        global_words += words;
    pthread_mutex_unlock(word_mutex);
    return NULL;
}

int main(int argc, char *argv[]){
    int NUM_THREADS = atoi(argv[1]);
    
    clock_t start, end;
    double execution_time;
    start = clock();
    
    int fd = open("Java.txt", O_RDONLY);
    long int file_length = lseek(fd, 0, SEEK_END); 
    long int chunk_size = file_length / NUM_THREADS + 1; 
    // due to integer division, NUM_THREADS * (file_length / NUM_THREADS) < file_length
    // last chunk will be oversize, but that beats skipping up to NUM_THREADS-1 characters at end of file.
    // I end up correcting the oversize chunk later. Last chunk will be slightly smaller than prior chunks
    // Potential difference between file_length - threads*(length/threads) will be spread throughout all chunks except last
    lseek(fd, 0, SEEK_SET);
    
    pthread_mutex_t words_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    pthread_t* threads = (pthread_t*) calloc(NUM_THREADS, sizeof(pthread_t));
    struct thread_args* all_args = (struct thread_args*) calloc(NUM_THREADS, sizeof(struct thread_args));
    
    char prev_char = ' ';
    for (int i = 0; i < NUM_THREADS; i++){
        
        all_args[i].word_mutex = &words_mutex;
        all_args[i].file_mutex = &file_mutex;
        all_args[i].file_descriptor = fd;
        all_args[i].start = i * chunk_size; // Each chunk starts at i*chunk_size
        all_args[i].length = chunk_size;    // And continues until (i+1)*chunk_size - 1
        all_args[i].last_chunk = 0;
        all_args[i].prev_char = prev_char; 
        
        // Special arguments for last chunk.
        if (i == NUM_THREADS - 1) {
            all_args[i].length = file_length - i * chunk_size; // Ensure chunk goes to end of file
            all_args[i].last_chunk = 1; 
            // If a chunk ends in a word, it counts on the thread with the next chunk to count it
            // Last chunk has no next thread. It should count the word if it ends in a word
        }
        
        pthread_create(&threads[i], NULL, reader_thread, &all_args[i]);
        
        pthread_mutex_lock(&file_mutex);
            lseek(fd, i*chunk_size+chunk_size-1, SEEK_SET);
            read(fd, &prev_char, 1);
        pthread_mutex_unlock(&file_mutex);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    end = clock();
    execution_time = ((double)(end-start));
    printf("Threads: %i \n", NUM_THREADS);
    printf("Words: %i \n", global_words);
    printf("Execution time: %.0f microseconds \n", execution_time);
    
    close(fd);
    free(all_args);
    free(threads);
    return 0;
}
