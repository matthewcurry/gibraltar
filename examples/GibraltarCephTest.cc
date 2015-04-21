// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 

#include <iomanip>
#include "GibraltarCephTest.h"
#include <gibraltar.h>

// Ceph include headers
#include <errno.h>

#include "crush/CrushWrapper.h"
#include "include/stringify.h"
#include "global/global_init.h"
#include "common/ceph_argparse.h"
#include "global/global_context.h"

/* May use these later
#include "erasure-code/gibraltar/ErasureCodeGibraltar.h"
#include "gtest/gtest.h"
*/

#ifndef LARGE_ENOUGH
#define LARGE_ENOUGH 2048
#endif

const unsigned GibraltarCephTest::SIMD_ALIGN = 32;

using namespace std;

void
GibraltarCephTest::run_test() {
  int j = 0;
  int iters = 5;
  cout << "Speed test with correctness checks" << endl;
  cout << "datasize is N*bufsize, or the total size of all data buffers"
       << endl;
  cout << "                         "
       << "cuda            cuda    " << endl;
  cout << "n    m       datasize    "
       << "chk_tput        rec_tput" << endl;

  const char *payload =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

  const char *zeros =
  "00000000000000000000000000000000000000000000000000000000000";

  const char *ones =
    "11111111111111111111111111111111111111111111111111111111111";

  for (unsigned int m = min_test; m <= max_test; m++) {
    for (unsigned int n = min_test; n <= max_test; n++) {
      double chk_time, dns_time;
      cout << n  << setw(4) << m;

      //cout << "step 1. m = " << m << " n = " << n << endl; 
      int rc = gib_init_cuda(n, m, &gc);
      if (rc) {
	cout << "Error: " << rc << endl;
	exit(EXIT_FAILURE);
      }

      //cout << "step 2a." << endl;
      int size = LARGE_ENOUGH;
      char * chunks[n+m];
      std::map<int,bufferlist> data;
      //cout << "step 2b." << endl;
      for(int i = 0; i < n; i++) {
	bufferptr in_ptr(buffer::create_aligned(LARGE_ENOUGH,SIMD_ALIGN));
	in_ptr.zero();
	in_ptr.set_length(0);
	//cout << "step 2b.i." << i << endl << flush;
	for (int j = 0;j<size/strlen(payload);j++) {
	  in_ptr.append(payload, strlen(payload));
	  //cout << "step 2b.i." << j << " length: " << in_ptr.length() << endl << flush;
	}
	unsigned int pad_size = size - in_ptr.length();
	string pad (i,pad_size);
	in_ptr.append(pad.c_str(),pad_size);
	//cout << "step 2b.i." << " ptr length: " << in_ptr.length() << endl << flush;
	bufferlist bl;
	data.insert(std::pair<int,bufferlist>(i,bl));
	data[i].push_front(in_ptr);
	// cout << "step 2b.ii " << i << " length of data: " << data[i].length() << endl << flush;
	data[i].rebuild_aligned_size_and_memory(LARGE_ENOUGH,SIMD_ALIGN);
	chunks[i] = data[i].c_str();
	/*
	//cout << "step 2b.iii." << i << endl << flush;
	cout << "Length data[" << i << "]: " << data[i].length() << endl;
	data[i].hexdump(cout);
	cout << endl << flush;
	*/
      }

      //cout << "step 2c." << endl << flush;
      for(int i = n; i < m + n; i++) {
	bufferptr in_ptr(buffer::create_aligned(LARGE_ENOUGH,SIMD_ALIGN));
	in_ptr.zero();
	in_ptr.set_length(0);
	for (int j = 0;j<size/strlen(zeros);j++) {
	  in_ptr.append(zeros, strlen(zeros));
	  //cout << "step 2b.i." << j << " length: " << in_ptr.length() << endl << flush;
	}
	unsigned int pad_size = size - in_ptr.length();
	char pad[pad_size + 1];
	strncpy(pad,ones,pad_size);
	pad[pad_size] = '\0';
	in_ptr.append(pad,pad_size);
	// cout << "step 2c.ii pad_size: " << pad_size << " length: " << in_ptr.length() << endl << flush;
	bufferlist bl;
	data.insert(std::pair<int,bufferlist>(i,bl));
	data[i].push_front(in_ptr);
	// cout << "step 2c.ii " << i << " length of data: " << data[i].length() << endl << flush;
	data[i].rebuild_aligned_size_and_memory(LARGE_ENOUGH,SIMD_ALIGN);
	chunks[i] = data[i].c_str();
	/*
	cout << "Length data[" << i << "]: " << data[i].length() << endl;
	data[i].hexdump(cout);
	cout << endl << flush;
	*/
      }

      //cout << "step 3." << endl;
      time_iters(chk_time, gib_generate2(chunks, size, gc), iters);

      //cout << "step 4a." << endl << flush;
      char * backup_chunks[n+m];
      std::map<int,bufferlist> backup_data;
      for (int i = 0; i < m + n; i++) {
	bufferlist bl;
	backup_data.insert(std::pair<int,bufferlist>(i,bl));
	//cout << "step 4a.i " << i << " length of data: " << data[i].length() << endl << flush;
	data[i].copy(0, size, backup_data[i]);
	backup_data[i].rebuild_aligned_size_and_memory(LARGE_ENOUGH,SIMD_ALIGN);
	backup_chunks[i] = backup_data[i].c_str();
	/*
	cout << "Length data[" << i << "]: " << data[i].length() << endl;
	data[i].hexdump(cout);
	cout << endl << flush;
	cout << "Length backup_data[" << i << "]: " << backup_data[i].length() << endl;
	backup_data[i].hexdump(cout);
	cout << endl << flush;
	*/
      }

      //cout << "step 4b." << endl << flush;
      char failed[256];
      for (unsigned int i = 0; i < n + m; i++)
	failed[i] = 0;
      for (int i = 0; i < ((m < n) ? m : n); i++) {
	int probe;
	do {
	  probe = rand() % n;
	} while (failed[probe] == 1);
	failed[probe] = 1;


	/* Destroy the buffer */
	data[i].zero(0,size);
      }

      //cout << "step 4c." << endl << flush;
      int buf_ids[256];
      int index = 0;
      int f_index = n;
      for (unsigned int i = 0; i < n; i++) {
	while (failed[index]) {
	  buf_ids[f_index++] = index;
	  index++;
	}
	buf_ids[i] = index;
	index++;
      }
      while (f_index != n + m) {
	buf_ids[f_index] = f_index;
	f_index++;
      }

      //cout << "step 4d." << endl << flush;
      char * dense_chunks[n+m];
      std::map<int,bufferlist> dense_data;
      for (int i = 0; i < m + n; i++) {
	bufferptr in_ptr(buffer::create_aligned(LARGE_ENOUGH,SIMD_ALIGN));
	bufferlist bl;
	dense_data.insert(std::pair<int,bufferlist>(i,bl));
	data[buf_ids[i]].copy(0, size, dense_data[i]);
	dense_data[i].rebuild_aligned_size_and_memory(LARGE_ENOUGH,SIMD_ALIGN);
	dense_chunks[i] = dense_data[i].c_str();
	/*
	cout << "Length data[" << i << "]: " << data[i].length() << endl;
	data[i].hexdump(cout);
	cout << endl << flush;
	cout << "Length dense_data[" << i << "]: " << dense_data[i].length() << endl;
	dense_data[i].hexdump(cout);
	cout << endl << flush;
	*/
      }

      int nfailed = (m < n) ? m : n;
      for (int i = n;i<nfailed;i++) {
	dense_data[i].zero(0, size);
	bufferptr in_ptr(buffer::create_aligned(LARGE_ENOUGH,SIMD_ALIGN));
	in_ptr.zero();
	in_ptr.set_length(0);
	string pad (i,size);
	in_ptr.append(pad.c_str(),size);
	dense_data[i].push_front(in_ptr);
	dense_data[i].rebuild_aligned_size_and_memory(LARGE_ENOUGH,SIMD_ALIGN);
	dense_chunks[i] = dense_data[i].c_str();
      }
      time_iters(dns_time,
		 gib_recover2(dense_chunks, size, buf_ids, nfailed, gc),
		 iters);

      //cout << "step 4e." << endl << flush;
      for (unsigned int i = 0; i < m + n; i++) {
	dense_data[i].rebuild_aligned_size_and_memory(LARGE_ENOUGH,SIMD_ALIGN);
	dense_chunks[i] = dense_data[i].c_str();
	if (memcmp(dense_chunks[i],
		   backup_chunks[buf_ids[i]],size)) {
	  cout << "Dense test failed on buffer " << i << " / " <<  buf_ids[i] << endl;
	  cout << "Length dense_data[" << i << "]: " << dense_data[i].length() << endl;
	  dense_data[i].hexdump(cout);
	  cout << endl << flush;
	  cout << "Length backup_data[" << buf_ids[i] << "]: " << backup_data[i].length() << endl;
	  backup_data[buf_ids[i]].hexdump(cout);
	  cout << endl << flush;
	  exit(1);
	}
      }


      //cout << "step 5." << endl;

      double size_mb = size * n / 1024.0 / 1024.0;

      if(j==0) cout << setw(10) << size * n;

      cout << setprecision(10);
      cout << setw(20) << size_mb / chk_time
	   << setw(20) << size_mb / dns_time << endl;

      //cout << "step 6." << endl;
      gib_destroy(gc);
    }
  }
}

int main(int argc,char **args) {
  GibraltarCephTest test;
  test.run_test();
  return 0;
}
