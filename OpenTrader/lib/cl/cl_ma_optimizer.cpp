#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <vector>

#include <CL/cl.h>
#include "stdafx.h"
#include "util.h"


#include "optimizer.h"
#include "indicator.h"

#include "cl_optimizer.h"

#include "cl_ma_optimizer.h"




Cl_ma_optimizer::Cl_ma_optimizer()
{
	input_ind_array_ = NULL;
	input_price_ = NULL;
	input_params_list_ = NULL;
	outputResult_ = NULL;
	strcpy(kernel_name_, "ma.cl");
	strcpy(kernel_function_name_, "ma_kernel");
}



Cl_ma_optimizer::~Cl_ma_optimizer()
{
	if (input_ind_array_)
		_aligned_free(input_ind_array_);
	if (input_price_)
		_aligned_free(input_price_);
	if (input_params_list_)
		_aligned_free(input_params_list_);
	if (outputResult_)
		_aligned_free(outputResult_);

}

int Cl_ma_optimizer::CreateAndInitBuffers()
{
	cl_uint maCount = 30;
	array_size_ = indicator_->GetCount();
	// allocate working buffers. 
	// the buffer should be aligned with 4K page and size should fit 64-byte cached line
	array_byte_size_ = ((sizeof(cl_int) * array_size_ - 1) / 64 + 1) * 64;
	ind_array_size_ = array_size_*maCount;
	ind_array_byte_size_ = ((sizeof(cl_int) * (ind_array_size_) - 1) / 64 + 1) * 64;
	LOGLINE("ma 1");
	input_ind_array_ = (cl_int*)_aligned_malloc(ind_array_byte_size_, 4096);
	LOGLINE("ma 1");
	input_price_ = (cl_int*)_aligned_malloc(array_byte_size_, 4096);
	LOGLINE("ma 1");
	if (NULL == input_ind_array_ || NULL == input_ind_array_)
	{
		LogError("Error: _aligned_malloc failed to allocate buffers.\n");
		return -1;
	}

	LOGLINE("ma 1");
	for (cl_uint i = 0; i < array_size_; ++i)
	{
		input_price_[i] = static_cast<int>(std::round(indicator_->GetPrice(i, PRICE_OPEN)*10000.00));
	}

	LOGLINE("ma 1");
	for (cl_uint i = 0; i < maCount; ++i)
		for (cl_uint j = 0; j < array_size_; ++j)
		{
			int idx = ((i)*array_size_) + j;
			input_ind_array_[idx] = static_cast<int>(std::round(indicator_->GetMa(j, i + 1, PRICE_OPEN)*10000.00));
		}

	LOGLINE("ma 1");
	test_option_count_ = 0;
	for (cl_uint i = 1; i < maCount; i++)
		for (cl_uint j = i + 1; j < maCount; j++)
		{

			ma_pair_list.push_back(MAKEWORD(i, j));
			test_option_count_++;
		}
	LOGLINE("ma 1");
	LogInfo("test_option_count_=%d\n", test_option_count_);
	params_byte_size_ = ((sizeof(cl_int) * test_option_count_ - 1) / 64 + 1) * 64;;
	LogInfo("params_byte_size_=%d\n", params_byte_size_);
	input_params_list_ = (cl_int*)_aligned_malloc(params_byte_size_, 4096);
	LOGLINE("ma 1");
	for (size_t i = 0; i < ma_pair_list.size(); i++)
		input_params_list_[i] = ma_pair_list[i];
	LOGLINE("ma 1");

	outputResult_ = (cl_int*)_aligned_malloc(params_byte_size_, 4096);
	for (size_t i = 0; i < ma_pair_list.size(); i++)
		outputResult_[i] = 0;
	LOGLINE("ma 1");
	return CL_SUCCESS;
}




/*
* Create OpenCL buffers from host memory
* These buffers will be used later by the OpenCL kernel_
*/
int  Cl_ma_optimizer::CreateBufferArguments()
{

	cl_int err = CL_SUCCESS;


	err = CreateAndInitBuffers();
	if (CL_SUCCESS != err)
	{
		LogError("Error: CreateAndInitBuffers returned \n");
		return err;
	}
	LOGLINE("1");
	// Create new OpenCL buffer objects
	// As these buffer are used only for read by the kernel, you are recommended to create it with flag CL_MEM_READ_ONLY.
	// Always set minimal read/write flags for buffers, it may lead to better performance because it allows runtime
	// to better organize data copying.
	// You use CL_MEM_COPY_HOST_PTR here, because the buffers should be populated with bytes at inputA and inputB.

	mem_ind_ = clCreateBuffer(context_, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(cl_uint) * ind_array_byte_size_, input_ind_array_, &err);
	if (CL_SUCCESS != err)
	{
		LogError("Error: clCreateBuffer for inputMA1 returned %s\n", TranslateOpenCLError(err));
		return err;
	}


	LOGLINE("1");
	mem_prices_ = clCreateBuffer(context_, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(cl_uint) * array_byte_size_, input_price_, &err);
	if (CL_SUCCESS != err)
	{
		LogError("Error: clCreateBuffer for inputPrice returned %s\n", TranslateOpenCLError(err));
		return err;
	}

	LOGLINE("1");
	mem_options_ = clCreateBuffer(context_, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(cl_uint) * params_byte_size_, input_params_list_, &err);
	if (CL_SUCCESS != err)
	{
		LogError("Error: clCreateBuffer for inputPrice returned %s\n", TranslateOpenCLError(err));
		return err;
	}


	// If the output buffer is created directly on top of output buffer using CL_MEM_USE_HOST_PTR,
	// then, depending on the OpenCL runtime implementation and hardware capabilities, 
	// it may save you not necessary data copying.
	// As it is known that output buffer will be write only, you explicitly declare it using CL_MEM_WRITE_ONLY.
	LOGLINE("1");
	mem_result_ = clCreateBuffer(context_, CL_MEM_READ_WRITE /*| CL_MEM_USE_HOST_PTR*/, sizeof(cl_uint) * params_byte_size_, /*outputResult_*/NULL, &err);
	if (CL_SUCCESS != err)
	{
		LogError("Error: clCreateBuffer for dstMem returned %s\n", TranslateOpenCLError(err));
		return err;
	}

	return CL_SUCCESS;
}

/*
* Set kernel_ arguments
*/
cl_uint Cl_ma_optimizer::SetKernelArguments()
{
	cl_int err = CL_SUCCESS;
	int index = 0;

	err = clSetKernelArg(kernel_, index++, sizeof(cl_mem), (void *)&mem_ind_);
	if (CL_SUCCESS != err)
	{
		LogError("error: Failed to set argument ma1, returned %s\n", TranslateOpenCLError(err));
		return err;
	}

	err = clSetKernelArg(kernel_, index++, sizeof(cl_mem), (void *)&mem_prices_);
	if (CL_SUCCESS != err)
	{
		LogError("Error: Failed to set argument prices_, returned %s\n", TranslateOpenCLError(err));
		return err;
	}

	err = clSetKernelArg(kernel_, index++, sizeof(cl_mem), (void *)&mem_options_);
	if (CL_SUCCESS != err)
	{
		LogError("Error: Failed to set argument options_, returned %s\n", TranslateOpenCLError(err));
		return err;
	}

	err = clSetKernelArg(kernel_, index++, sizeof(cl_mem), (void *)&mem_result_);
	if (CL_SUCCESS != err)
	{
		LogError("Error: Failed to set argument prices_, returned %s\n", TranslateOpenCLError(err));
		return err;
	}
	err = clSetKernelArg(kernel_, index++, sizeof(cl_int), (void *)&array_size_);
	if (CL_SUCCESS != err)
	{
		LogError("Error: Failed to set argument size, returned %s\n", TranslateOpenCLError(err));
		return err;
	}
	err = clSetKernelArg(kernel_, index++, sizeof(cl_int), (void *)&ind_array_size_);
	if (CL_SUCCESS != err)
	{
		LogError("Error: Failed to set argument size, returned %s\n", TranslateOpenCLError(err));
		return err;
	}

	return err;
}


/*
* "Read" the result_ buffer (mapping the buffer to the host memory address)
*/
long Cl_ma_optimizer::ReadAndVerify(tradeParameters* optResult)
{
	cl_int err = CL_SUCCESS;
	bool ret = true;
	cl_int *resultPtr;
	// Enqueue a command to map the buffer object (dstMem) into the host address space and returns a pointer to it
	// The map operation is blocking
//	resultPtr = (cl_int *)clEnqueueMapBuffer(command_queue_, this->mem_result_,
//		true, CL_MAP_READ, 0, sizeof(cl_uint) * result_size_, 0, NULL, NULL, &err);

	if (CL_SUCCESS != err)
	{
		LogError("Error: clEnqueueMapBuffer returned %s\n", TranslateOpenCLError(err));
		return false;
	}

	// Call clFinish to guarantee that output region is updated
	err = clFinish(command_queue_);
	if (CL_SUCCESS != err)
	{
		LogError("Error: clFinish returned %s\n", TranslateOpenCLError(err));
	}

	resultPtr = (cl_int*)_aligned_malloc(sizeof(cl_int) * test_option_count_,4096);
	// Unmapped the output buffer before releasing it
//	err = clEnqueueUnmapMemObject(command_queue_, mem_result_, resultPtr, 0, NULL, NULL);
	err = clEnqueueReadBuffer(command_queue_, mem_result_, CL_TRUE, 0, sizeof(cl_int) * test_option_count_, resultPtr, 0, NULL, NULL);
	if (CL_SUCCESS != err)
	{
		LogError("Error: clEnqueueUnmapMemObject returned %s\n", TranslateOpenCLError(err));
	}

	long temp = 0;
	long best = -100000;
	for (cl_int i = 0; i < test_option_count_; ++i)
	{
		temp = resultPtr[i];
		int ma1 = (ma_pair_list[i] & 0xFF);
		int ma2 = ((ma_pair_list[i] >> 8) & 0xFF);
//		printf("host -profit 1 [%d,%d]=%ld\n", ma1, ma2, temp);
		if (best < temp) {
			optResult->params[0] = ma1;
			optResult->params[1] = ma2;
			best = temp;
		}
	}
	optResult->counts = 2;
	_aligned_free(resultPtr);
	return best;
}



