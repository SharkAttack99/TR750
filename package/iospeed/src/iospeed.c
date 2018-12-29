#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE (10000 * 512)

int main(int argc, char *argv[])
{
    int totalSize, chunkSize, bytesToWrite, bytesToRead;
    size_t bytesWritten, bytesRead;
    FILE *file;
    uint8_t buffer[MAX_BUFFER_SIZE];
    struct timeval t1, t2;
    double elapsedTime;
    const char *filePath;

    // read in args
    if (argc < 3)
    {
        printf("usage: ioTest <size to write in bytes> <path to file>\n");
        return 0;
    }

    if (sscanf(argv[1], "%d", &totalSize) != 1)
    {
        printf("Failed to read total size\n");
        return 1;
    }
    filePath = argv[2];

    printf("Read and write %d bytes to %s\n", totalSize, filePath);

    file = fopen(filePath, "w+");
    if (!file)
    {
        printf("Failed to open %s", filePath);
        return 1;
    }

    printf("\n\nWrite Speed Test:\n");
    // start timer
    gettimeofday(&t1, NULL);

    bytesToWrite = totalSize;
    while (bytesToWrite > 0)
    {
        chunkSize = MAX_BUFFER_SIZE;
        if (bytesToWrite < chunkSize)
        {
            chunkSize = bytesToWrite;
        }

        printf("Write %d bytes...\n", chunkSize);
        bytesWritten = fwrite(buffer, 1, chunkSize, file);
        if (bytesWritten < 0)
        {
            printf("fwrite failed: %d\n", bytesWritten);
            return 1;
        }

        bytesToWrite -= bytesWritten;
    }
    fflush(file);
    // sync cache
    system("sync; echo 3 > /proc/sys/vm/drop_caches");
    fclose(file);

    // stop timer
    gettimeofday(&t2, NULL);
    elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;    // sec to ms
    elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0; // us to ms
    float megaBytes = (float)totalSize / 1000000.0f;
    float seconds = (float)elapsedTime / 1000.0f;
    printf("Wrote %.1f MB in %.1f ms (%.1f MB/s)\n", megaBytes, seconds, megaBytes / seconds);

    // open for reading
    file = fopen(filePath, "r");
    if (!file)
    {
        printf("Failed to open %s", filePath);
        return 1;
    }

    printf("\n\nRead Speed Test:\n");
    // start timer
    gettimeofday(&t1, NULL);

    bytesToRead = totalSize;
    while (bytesToRead > 0)
    {
        chunkSize = MAX_BUFFER_SIZE;
        if (bytesToRead < chunkSize)
        {
            chunkSize = bytesToRead;
        }

        printf("Rrite %d bytes...\n", chunkSize);
        bytesRead = fread(buffer, 1, chunkSize, file);
        if (bytesRead < 0)
        {
            printf("fread failed: %d\n", bytesRead);
            return 1;
        }

        bytesToRead -= bytesRead;
    }
    fclose(file);

    // stop timer
    gettimeofday(&t2, NULL);
    elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;    // sec to ms
    elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0; // us to ms
    megaBytes = (float)totalSize / 1000000.0f;
    seconds = (float)elapsedTime / 1000.0f;
    printf("Read %.1f MB in %.1f ms (%.1f MB/s)\n", megaBytes, seconds, megaBytes / seconds);

    return 0;
}
