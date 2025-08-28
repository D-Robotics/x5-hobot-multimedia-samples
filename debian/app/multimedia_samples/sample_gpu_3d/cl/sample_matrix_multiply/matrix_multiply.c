#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>
#include <math.h> // for fabs function
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strcmp function
#include <time.h>

#define MATRIX_SIZE 512 // Reduced matrix size to reduce computation time
#define EPSILON 1e-5

// Helper function to check OpenCL errors
void checkErr(cl_int err, const char *name) {
  if (err != CL_SUCCESS) {
    fprintf(stderr, "ERROR: %s (%d)\n", name, err);
    exit(EXIT_FAILURE);
  }
}

// Function to print a matrix with a table-like format
void print_matrix(const char *name, float *M, int n, int print_limit) {
  printf("Matrix %s:\n", name);
  int limit = (print_limit > 0 && print_limit < n) ? print_limit : n;

  // Print column headers
  printf("      ");
  for (int j = 0; j < limit; j++) {
    printf("| Col %-4d ", j);
  }
  printf("|\n");

  // Print separator
  printf("------");
  for (int j = 0; j < limit; j++) {
    printf("|==========");
  }
  printf("|\n");

  // Print matrix rows
  for (int i = 0; i < limit; i++) {
    printf("Row %-4d ", i);
    for (int j = 0; j < limit; j++) {
      printf("| %8.2f ", M[i * n + j]);
    }
    printf("|\n");
  }
  printf("\n");
}

// CPU implementation of matrix multiplication
void matrix_mult_cpu(float *A, float *B, float *C, int n) {
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      C[i * n + j] = 0;
      for (int k = 0; k < n; k++) {
        C[i * n + j] += A[i * n + k] * B[k * n + j];
      }
    }
  }
}

// OpenCL implementation of matrix multiplication
void matrix_mult_opencl(float *A, float *B, float *C, int n) {
  cl_int err;
  cl_platform_id platform_id;
  cl_device_id device_id;
  cl_context context;
  cl_command_queue queue;
  cl_program program;
  cl_kernel kernel;

  // Get platform and device information
  err = clGetPlatformIDs(1, &platform_id, NULL);
  checkErr(err, "clGetPlatformIDs");

  err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, NULL);
  checkErr(err, "clGetDeviceIDs");

  // Create OpenCL context
  context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &err);
  checkErr(err, "clCreateContext");

  // Create command queue
  queue = clCreateCommandQueue(context, device_id, 0, &err);
  checkErr(err, "clCreateCommandQueue");

  // Create memory buffers on the device
  cl_mem bufferA = clCreateBuffer(context, CL_MEM_READ_ONLY,
                                  n * n * sizeof(float), NULL, &err);
  checkErr(err, "clCreateBuffer A");
  cl_mem bufferB = clCreateBuffer(context, CL_MEM_READ_ONLY,
                                  n * n * sizeof(float), NULL, &err);
  checkErr(err, "clCreateBuffer B");
  cl_mem bufferC = clCreateBuffer(context, CL_MEM_WRITE_ONLY,
                                  n * n * sizeof(float), NULL, &err);
  checkErr(err, "clCreateBuffer C");

  // Copy the matrices A and B to their respective memory buffers
  err = clEnqueueWriteBuffer(queue, bufferA, CL_TRUE, 0, n * n * sizeof(float),
                             A, 0, NULL, NULL);
  checkErr(err, "clEnqueueWriteBuffer A");
  err = clEnqueueWriteBuffer(queue, bufferB, CL_TRUE, 0, n * n * sizeof(float),
                             B, 0, NULL, NULL);
  checkErr(err, "clEnqueueWriteBuffer B");

  // Read and build the kernel program
  const char *programSource =
      "__kernel void matrix_mult(__global float* A, __global float* B, "
      "__global float* C, int n) {"
      "    int row = get_global_id(1);"
      "    int col = get_global_id(0);"
      "    float value = 0;"
      "    for (int k = 0; k < n; k++) {"
      "        value += A[row * n + k] * B[k * n + col];"
      "    }"
      "    C[row * n + col] = value;"
      "}";

  program = clCreateProgramWithSource(context, 1, &programSource, NULL, &err);
  checkErr(err, "clCreateProgramWithSource");

  err = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
  if (err != CL_SUCCESS) {
    // Print build log in case of error
    size_t log_size;
    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL,
                          &log_size);
    char *log = (char *)malloc(log_size);
    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, log_size,
                          log, NULL);
    fprintf(stderr, "ERROR: %s\n", log);
    free(log);
    checkErr(err, "clBuildProgram");
  }

  // Create the OpenCL kernel
  kernel = clCreateKernel(program, "matrix_mult", &err);
  checkErr(err, "clCreateKernel");

  // Set the arguments of the kernel
  err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferA);
  checkErr(err, "clSetKernelArg 0");
  err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufferB);
  checkErr(err, "clSetKernelArg 1");
  err = clSetKernelArg(kernel, 2, sizeof(cl_mem), &bufferC);
  checkErr(err, "clSetKernelArg 2");
  err = clSetKernelArg(kernel, 3, sizeof(int), &n);
  checkErr(err, "clSetKernelArg 3");

  // Execute the OpenCL kernel
  size_t global_item_size[2] = {n, n};
  size_t local_item_size[2] = {16, 16}; // Adjust as needed
  err = clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global_item_size,
                               local_item_size, 0, NULL, NULL);
  checkErr(err, "clEnqueueNDRangeKernel");

  // Read the memory buffer C on the device to the local variable C
  err = clEnqueueReadBuffer(queue, bufferC, CL_TRUE, 0, n * n * sizeof(float),
                            C, 0, NULL, NULL);
  checkErr(err, "clEnqueueReadBuffer");

  // Clean up
  clFlush(queue);
  clFinish(queue);
  clReleaseKernel(kernel);
  clReleaseProgram(program);
  clReleaseMemObject(bufferA);
  clReleaseMemObject(bufferB);
  clReleaseMemObject(bufferC);
  clReleaseCommandQueue(queue);
  clReleaseContext(context);
}

// Function to compare two matrices and print any differences
void compare_matrices(float *C_cpu, float *C_gpu, int n, int verbose) {
  int differences = 0;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      if (fabs(C_cpu[i * n + j] - C_gpu[i * n + j]) > EPSILON) {
        differences++;
        if (verbose) {
          printf("Difference at (%d, %d): CPU = %f, GPU = %f\n", i, j,
                 C_cpu[i * n + j], C_gpu[i * n + j]);
        }
      }
    }
  }
  if (differences == 0) {
    printf("Matrices are identical!\n");
  } else {
    printf("Found %d differences between CPU and GPU results.\n", differences);
  }

}

int main(int argc, char **argv) {
  int verbose = 0;
  int print_limit = MATRIX_SIZE;

  // Allocate memory for matrices A, B, C_cpu, and C_gpu
  float *A = (float *)malloc(sizeof(float) * MATRIX_SIZE * MATRIX_SIZE);
  float *B = (float *)malloc(sizeof(float) * MATRIX_SIZE * MATRIX_SIZE);
  float *C_cpu = (float *)malloc(sizeof(float) * MATRIX_SIZE * MATRIX_SIZE);
  float *C_gpu = (float *)malloc(sizeof(float) * MATRIX_SIZE * MATRIX_SIZE);

  // Initialize matrices A and B with some values
  for (int i = 0; i < MATRIX_SIZE; i++) {
    for (int j = 0; j < MATRIX_SIZE; j++) {
      A[i * MATRIX_SIZE + j] = (float)(rand() % 100);
      B[i * MATRIX_SIZE + j] = (float)(rand() % 100);
    }
  }

  // CPU execution
  clock_t start_cpu = clock();
  matrix_mult_cpu(A, B, C_cpu, MATRIX_SIZE);
  clock_t end_cpu = clock();
  double time_cpu = ((double)(end_cpu - start_cpu)) / CLOCKS_PER_SEC;
  printf("CPU execution time: %f seconds\n", time_cpu);

  // OpenCL execution
  clock_t start_opencl = clock();
  matrix_mult_opencl(A, B, C_gpu, MATRIX_SIZE);
  clock_t end_opencl = clock();
  double time_opencl = ((double)(end_opencl - start_opencl)) / CLOCKS_PER_SEC;
  printf("OpenCL execution time: %f seconds\n", time_opencl);

  // Print matrices if verbose flag is set
  if (verbose) {
    print_matrix("C_CPU", C_cpu, MATRIX_SIZE, print_limit);
    print_matrix("C_GPU", C_gpu, MATRIX_SIZE, print_limit);
  }

  // Compare CPU and GPU results
  compare_matrices(C_cpu, C_gpu, MATRIX_SIZE, verbose);

  // Clean up
  free(A);
  free(B);
  free(C_cpu);
  free(C_gpu);

  return 0;
}