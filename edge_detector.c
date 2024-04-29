#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>

#define LAPLACIAN_THREADS 4  //change the number of threads as you run your concurrency experiment

/* Laplacian filter is 3 by 3 */
#define FILTER_WIDTH 3       
#define FILTER_HEIGHT 3      

#define RGB_COMPONENT_COLOR 255

typedef struct {
      unsigned char r, g, b;
} PPMPixel;

struct parameter {
    PPMPixel *image;         //original image pixel data
    PPMPixel *result;        //filtered image pixel data
    unsigned long int w;     //width of image
    unsigned long int h;     //height of image
    unsigned long int start; //starting point of work
    unsigned long int size;  //equal share of work (almost equal if odd)
};


struct file_name_args {
    char *input_file_name;      //e.g., file1.ppm
    char output_file_name[20];  //will take the form laplaciani.ppm, e.g., laplacian1.ppm
};


/*The total_elapsed_time is the total time taken by all threads 
to compute the edge detection of all input images .
*/
double total_elapsed_time = 0; 
pthread_mutex_t mutex_total_time = PTHREAD_MUTEX_INITIALIZER;


/*This is the thread function. It will compute the new values for the region of image specified in params (start to start+size) using convolution.
    For each pixel in the input image, the filter is conceptually placed on top ofthe image with its origin lying on that pixel.
    The  values  of  each  input  image  pixel  under  the  mask  are  multiplied  by the corresponding filter values.
    Truncate values smaller than zero to zero and larger than 255 to 255.
    The results are summed together to yield a single output value that is placed in the output image at the location of the pixel being processed on the input.
 
 */
void *compute_laplacian_threadfn(void *params)
{

    struct parameter *param = (struct parameter *)params;

    // Extract necessary data from the parameter struct
    PPMPixel *image = param->image; // Original image pixel data
    PPMPixel *result = param->result; // Filtered image pixel data
    unsigned long int w = param->w; // Width of the image
    unsigned long int h = param->h; // Height of the image
    unsigned long int start = param->start; 
    unsigned long int size = param->size; 

    int laplacian[FILTER_WIDTH][FILTER_HEIGHT] = {
        {-1, -1, -1},
        {-1,  8, -1},
        {-1, -1, -1}
    };


    // Iterate over the portion of the image assigned to this thread
    for (unsigned long int i = start; i < start + size && i < h; ++i) {
        for (unsigned long int j = 0; j < w; ++j) {
            int red = 0, green = 0, blue = 0;
            // Iterate over the filter matrix
            for (int fi = 0; fi < FILTER_HEIGHT; ++fi) {
                for (int fj = 0; fj < FILTER_WIDTH; ++fj) {
                    // Calculate the position of the pixel to be accessed
                    int x = (int)j + fj - FILTER_WIDTH / 2;
                    int y = (int)i + fi - FILTER_HEIGHT / 2;
                    // Handle boundary conditions to avoid accessing out-of-bounds pixels
                    if (x >= 0 && y >= 0 && x < w && y < h) {
                        // Access the pixel data from the original image
                        PPMPixel *pixel = &image[y * w + x];
                        // Apply the filter to the pixel data
                        red += pixel->r * laplacian[fi][fj];
                        green += pixel->g * laplacian[fi][fj];
                        blue += pixel->b * laplacian[fi][fj];
                    }
                }
            }
            
            PPMPixel *targetPixel = &result[i * w + j];
            targetPixel->r = (unsigned char) fmax(0, fmin(255, red));
            targetPixel->g = (unsigned char) fmax(0, fmin(255, green));
            targetPixel->b = (unsigned char) fmax(0, fmin(255, blue));
        }
    }
    return NULL; 
}






/* Apply the Laplacian filter to an image using threads.
 Each thread shall do an equal share of the work, i.e. work=height/number of threads. If the size is not even, the last thread shall take the rest of the work.
 Compute the elapsed time and store it in *elapsedTime (Read about gettimeofday).
 Return: result (filtered image)
 */
PPMPixel *apply_filters(PPMPixel *image, unsigned long w, unsigned long h, double *elapsedTime) {
    struct timeval start_time, end_time;

    // Allocate memory for the result image
    PPMPixel *result = (PPMPixel *)malloc(w * h * sizeof(PPMPixel));
    if (!result) {
            fprintf(stderr, "Memory allocation failed\n");
            return NULL;
        }
    // Calculate the workload for each thread
    unsigned long work = h / LAPLACIAN_THREADS;
    pthread_t threads[LAPLACIAN_THREADS];
    struct parameter params[LAPLACIAN_THREADS];

    // Record start time
    gettimeofday(&start_time, NULL);

    // Create threads
    for (int i = 0; i < LAPLACIAN_THREADS; i++) {
        // Set parameters for each thread
        params[i].image = image;
        params[i].result = result;
        params[i].w = w;
        params[i].h = h;
        params[i].start = i * work;
        params[i].size = (i == LAPLACIAN_THREADS - 1) ? (h - (i * work)) : work; // Adjust the last thread's workload

        // Create thread and pass parameters
        if (pthread_create(&threads[i], NULL, compute_laplacian_threadfn, (void *)&params[i])) {
            fprintf(stderr, "Error creating thread\n");
            free(result); // Free allocated memory before returning
            return NULL;
        }
    }

    // Join threads
    for (int i = 0; i < LAPLACIAN_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Record end time
    gettimeofday(&end_time, NULL);

    // Calculate elapsed time
    *elapsedTime = (end_time.tv_sec - start_time.tv_sec) * 1000.0; // Convert seconds to milliseconds
    *elapsedTime += (end_time.tv_usec - start_time.tv_usec) / 1000.0; // Convert microseconds to milliseconds

    return result; // Return the filtered image
}


/*Create a new P6 file to save the filtered image in. Write the header block
 e.g. P6
      Width Height
      Max color value
 then write the image data.
 The name of the new file shall be "filename" (the second argument).
 */
void write_image(PPMPixel *image, char *filename, unsigned long int width, unsigned long int height)
{
    // Open the file for writing in binary mode
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Unable to open file '%s' for writing\n", filename);
        return;
    }

    // Write the header information to the file
    fprintf(fp, "P6\n%lu %lu\n255\n", width, height);
    // P6 format identifier, width, height, and maximum color value (255)

    // Write the image data to the file
    // Note: fwrite writes the entire image data in one go
    if (fwrite(image, sizeof(PPMPixel), width * height, fp) != width * height) {
        fprintf(stderr, "Error writing image data to file\n");
    }

    // Close the file
    fclose(fp);
    
}



/* Open the filename image for reading, and parse it.
    Example of a ppm header:    //http://netpbm.sourceforge.net/doc/ppm.html
    P6                  -- image format
    # comment           -- comment lines begin with
    ## another comment  -- any number of comment lines
    200 300             -- image width & height 
    255                 -- max color value
 
 Check if the image format is P6. If not, print invalid format error message.
 If there are comments in the file, skip them. You may assume that comments exist only in the header block.
 Read the image size information and store them in width and height.
 Check the rgb component, if not 255, display error message.
 Return: pointer to PPMPixel that has the pixel data of the input image (filename).The pixel data is stored in scanline order from left to right (up to bottom) in 3-byte chunks (r g b values for each pixel) encoded as binary numbers.
 */
PPMPixel *read_image(const char *filename, unsigned long int *width, unsigned long int *height)
{
    // Open file for reading
    FILE *fp = fopen(filename, "rb");

    if (!fp){
         fprintf(stderr, "Unable to open file %s\n", filename);
        return NULL;
    }

    // Read the header
    char header[3];
    if (fscanf(fp, "%2s", header) != 1 || strcmp(header, "P6") != 0) {
        fprintf(stderr, "Invalid image format (must be 'P6')\n");
        fclose(fp);
        return NULL;
    }


    // Skip comments and any additional whitespace in the header
    int c;
    do {
        while (isspace(c = fgetc(fp))); // Skip whitespace
        if (c == '#') { // If it's a comment, skip the rest of the line
            while ((c = fgetc(fp)) != '\n' && c != EOF);
        } else {
            ungetc(c, fp); // If not a comment, put the character back for reading width and height
            break;
        }
    } while (c != EOF);

    // Read image size information
    if (fscanf(fp, "%lu %lu", width, height) != 2) {
        fprintf(stderr, "Invalid image size\n");
        fclose(fp);
        return NULL;
    }

    // Skip whitespace to get to the maxval
    do {
        c = fgetc(fp);
    } while (isspace(c));

    // Read and check the rgb component
    unsigned int maxval;
    ungetc(c, fp); // Put back the first digit of maxval
    if (fscanf(fp, "%u", &maxval) != 1 || maxval != 255) {
        fprintf(stderr, "Invalid RGB component (must be 255)\n");
        fclose(fp);
        return NULL;
    }

    // There should be exactly one whitespace character (typically a newline) after the maxval and before the binary data
    if (fgetc(fp) != '\n') {
        fprintf(stderr, "Header formatting error\n");
        fclose(fp);
        return NULL;
    }

    // Allocate memory for pixel data
    PPMPixel *img = (PPMPixel *)malloc((*width) * (*height) * sizeof(PPMPixel));
    // Read pixel data
    if (fread(img, sizeof(PPMPixel), (*width) * (*height), fp) != (*width) * (*height)) {
        fprintf(stderr, "Error reading pixel data\n");
        free(img);
        fclose(fp);
        return NULL;
    }
    // Close and return pixle data
    fclose(fp);
    return img;
}


/* The thread function that manages an image file. 
 Read an image file that is passed as an argument at runtime. 
 Apply the Laplacian filter. 
 Update the value of total_elapsed_time.
 Save the result image in a file called laplaciani.ppm, where i is the image file order in the passed arguments.
 Example: the result image of the file passed third during the input shall be called "laplacian3.ppm".

*/
void *manage_image_file(void *args) {
    // Cast the input arguments to the appropriate struct type
    struct file_name_args *fileArgs = (struct file_name_args *)args;

    // Variables to store image width, height, and elapsed time
    unsigned long int width, height;
    double elapsedTime = 0;

    // Read the input image
    PPMPixel *image = read_image(fileArgs->input_file_name, &width, &height);
    if (!image) {
        // If reading the image fails, print an error message and return NULL
        fprintf(stderr, "Failed to read image %s\n", fileArgs->input_file_name);
        return NULL;
    }

    // Apply the Laplacian filter to the image
    PPMPixel *filteredImage = apply_filters(image, width, height, &elapsedTime);
    if (!filteredImage) {
        // If applying filters fails, print an error message, free memory, and return NULL
        fprintf(stderr, "Failed to apply filters to image %s\n", fileArgs->input_file_name);
        free(image); // Free memory allocated for the original image
        return NULL;
    }

    // Properly synchronized update of the total_elapsed_time
    pthread_mutex_lock(&mutex_total_time);
    total_elapsed_time += elapsedTime; // Add the elapsedTime to the global total (ensure elapsedTime is in milliseconds before this step)
    pthread_mutex_unlock(&mutex_total_time);

    // Update the total elapsed time (assuming total_elapsed_time is accessible and properly synchronized)
    printf("Total elapsed time for %s: %.4f s\n", fileArgs->input_file_name ,elapsedTime /1000);
    //total_elapsed_time += elapsedTime;

    // Save the filtered image to a file
    write_image(filteredImage, fileArgs->output_file_name, width, height);

    // Free allocated memory for both the original and filtered images
    free(image);
    free(filteredImage);

    return NULL;
}
/*The driver of the program. Check for the correct number of arguments. If wrong print the message: "Usage ./a.out filename[s]"
  It shall accept n filenames as arguments, separated by whitespace, e.g., ./a.out file1.ppm file2.ppm    file3.ppm
  It will create a thread for each input file to manage.  
  It will print the total elapsed time in .4 precision seconds(e.g., 0.1234 s). 
 */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s filename[s]\n", argv[0]);
        return 1;
    }

    pthread_t threads[argc - 1];
    struct file_name_args args[argc - 1];

    // Create a thread for each file
    for (int i = 1; i < argc; i++) {
        args[i - 1].input_file_name = argv[i];
        sprintf(args[i - 1].output_file_name, "laplacian%d.ppm", i);
        if (pthread_create(&threads[i - 1], NULL, manage_image_file, (void *)&args[i - 1])) {
            fprintf(stderr, "Error creating thread for file %s\n", argv[i]);
            return 2;
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < argc - 1; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Total elapsed time: %.4f s\n", total_elapsed_time / 1000.0);

    // optional
    pthread_mutex_destroy(&mutex_total_time);

    return 0;
}
