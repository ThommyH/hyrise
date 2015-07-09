#pragma once

/*
 * XswBlockManager.h
 *
 *  Created on: May 12, 2014
 *      Author: Jan Ole Vollmer
 */

#ifdef WITH_XSW

#include <sys/types.h>
#include "xsw_api/xsw_apis.h"

#include <cstddef>
#include <string>
#include <vector>
#include <map>

namespace hyrise {
namespace memory {

class XswBlockManager {
public:
  static XswBlockManager* getDefault();

public:
  XswBlockManager();
  XswBlockManager(const XswBlockManager&) = delete;
  XswBlockManager(XswBlockManager&&) = delete;
  virtual ~XswBlockManager();

  XswBlockManager& operator=(const XswBlockManager&) = delete;

  void* allocate(std::size_t numBytes);
  void deallocate(void* ptr, std::size_t numBytes);

  XSW_BIO* mmap();

  std::size_t capacity() const;
  std::size_t remaining() const;
  void reset();

private:
  class PageCache {
  public:
    PageCache();
    ~PageCache();

    const std::string& tag() const  { return m_tag; }

  private:
    enum Type {
      k_lru = XSW_PAGE_CACHE_LRU,
      k_fifo = XSW_PAGE_CACHE_FIFO,
      k_invalid = 0xFFFF
    };

    void configureFromEnv();
    void createPageCache();
    void deletePageCache();

    std::string m_tag;
    std::size_t m_size;
    std::size_t m_lowWaterLevel;
    std::size_t m_evictionSize;
    Type m_type;

    static const std::map<std::string, Type> s_pageCacheTypeMap;
  };

private:
  void configureFromEnv();
  void loadModules();
  void createMmap();

private:
  std::vector<std::string> m_modules;
  std::string m_deviceName;
  std::string m_regionName;
  XSW_BIO* m_deviceHandle;
  XSW_BIO* m_regionHandle;
  XSW_BIO* m_mmap;
  std::size_t m_currentPosition;
  PageCache* m_pageCache;
  int m_minReadahead;
  int m_maxReadahead;
  bool m_debugMode;
};

}  // namespace memory
}  // namespace hyrise

#endif // WITH_XSW
