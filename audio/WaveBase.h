#ifndef _WAVEBASE_H_
#define _WAVEBASE_H_

#include <vector>
#define NUM_BUF 10

typedef void(*PcmCallback)(unsigned char* pcm, int size, void* user_data);
typedef struct wavehdr_tag WAVEHDR;

class WaveBase {
public:
  void SetPcmCallback(PcmCallback func, void* data);
protected:
  virtual ~WaveBase();
  PcmCallback cb_func;
  void* cb_data;

  virtual void OnWaveDone(WAVEHDR* hdr) = 0;
  virtual bool OnWaveAlloc(WAVEHDR* hdr) = 0;
  virtual void OnWaveFree(WAVEHDR* hdr) = 0;
  /// 释放缓冲区
  bool FreeBuffer();
  /// 申请缓冲区
  bool AllocBuffer(int count);

  std::vector<WAVEHDR> m_pHdr; ///< 缓冲区数组(默认有10个缓冲区)
  unsigned int m_nBufSize; ///< 单个缓冲区大小
};

#endif