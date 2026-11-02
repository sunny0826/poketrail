#pragma once
#include "harness.hpp"
#include "file_store.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
struct Slots : SlotIO {
  std::vector<uint8_t> files[2], attempt;bool before=false,after=false,readback=false;size_t cut=SIZE_MAX;unsigned writes=0;
  LoadStatus prepare() override{return LoadStatus::Loaded;}
  ReadStatus read(unsigned slot,uint8_t* out,size_t cap,size_t& n) override {
    if(readback && writes){readback=false;return ReadStatus::IoError;}
    n=files[slot].size();if(!n)return ReadStatus::Missing;
    std::copy(files[slot].begin(),files[slot].begin()+std::min(cap,n),out);
    return n>cap?ReadStatus::TooLarge:ReadStatus::Ok;
  }
  bool write(unsigned slot,const uint8_t* b,size_t n) override {
    ++writes;attempt.assign(b,b+n);if(before)return false;files[slot].assign(b,b+std::min(cut,n));return !after&&cut>=n;
  }
};
static std::vector<uint8_t> legacy(unsigned kind) {
  char path[128];std::snprintf(path,sizeof(path),"test/fixtures/progression/v1-%u.bin",kind);
  std::ifstream file(path,std::ios::binary);CHECK(bool(file));
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(file),{});
}
static std::unique_ptr<Save> decoded(const std::vector<uint8_t>& bytes) {
  std::unique_ptr<Save> s(new Save);CHECK(codec::decode(bytes.data(),bytes.size(),*s)==codec::DecodeStatus::Ok);return s;
}
