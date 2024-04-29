# Laplacian-Image-Filter

The Laplacian Image Filter provides an implementation of a parallel Laplacian filter using Pthreads for multithreading. The Laplacian filter is a commonly used technique in image processing for edge detection. By applying convolution with a 3x3 filter matrix, it enhances the edges within an image.

The implementation utilizes Pthreads to distribute the workload across multiple threads, thereby accelerating the filtering process, especially for large images. Each thread processes a portion of the image independently, and the results are combined to produce the final filtered image.
