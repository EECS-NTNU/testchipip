#include "testchip_tsi.h"
#include <stdexcept>
#include <stdlib.h>
#include <iostream>

testchip_tsi_t::testchip_tsi_t(int argc, char** argv, bool can_have_loadmem) : tsi_t(argc, argv)
{
  has_loadmem = false;
  init_writes = std::vector<std::pair<uint64_t, uint32_t>>();
  write_hart0_msip = true;

  std::vector<std::string> args(argv + 1, argv + argc);
  for (auto& arg : args) {
    if (arg.find("+loadmem=") == 0)
      has_loadmem = can_have_loadmem;
    if (arg.find("+init_write=0x") == 0) {
      auto d = arg.find(":0x");
      if (d == std::string::npos) {
        throw std::invalid_argument("Improperly formatted +init_write argument");
      }
      uint64_t addr = strtoull(arg.substr(14, d - 14).c_str(), 0, 16);
      uint32_t val = strtoull(arg.substr(d + 3).c_str(), 0, 16);

      init_writes.push_back(std::make_pair(addr, val));
    }
    if (arg.find("+no_hart0_msip") == 0)
      write_hart0_msip = false;
  }

  try{
    shm = shared_memory_object(open_only, "vortex_shared_memory", read_write);
    region = mapped_region(shm, read_write);
    smo = static_cast<Smo*>(region.get_address());
    scoped_lock<interprocess_mutex> lock(smo->mutex);
    smo->condition.notify_one();
    using_ipc_driver = true;
  }
  catch (std::exception){
    using_ipc_driver = false;
  }

}

void testchip_tsi_t::idle(){
  if(using_ipc_driver) parse_command();
  switch_to_target();
}

void testchip_tsi_t::parse_command(){
  scoped_lock<interprocess_mutex> lock(smo->mutex, try_to_lock);
  if(!lock.owns()) {
    return;
  }
  switch (smo->command)
    {
    case READ:
      smo->command = NONE;
      memif().read(smo->address, smo->size, smo->data);
      smo->condition.notify_one();
      break;
    case WRITE:
      smo->command = NONE;
      memif().write(smo->address, smo->size, smo->data);
      smo->condition.notify_one();
      break;
    }
}

void testchip_tsi_t::write_chunk(addr_t taddr, size_t nbytes, const void* src)
{
  if (is_loadmem) {
    load_mem_write(taddr, nbytes, src);
  } else {
    tsi_t::write_chunk(taddr, nbytes, src);
  }
}

void testchip_tsi_t::read_chunk(addr_t taddr, size_t nbytes, void* dst)
{
  if (is_loadmem) {
    load_mem_read(taddr, nbytes, dst);
  } else {
    tsi_t::read_chunk(taddr, nbytes, dst);
  }
}

void testchip_tsi_t::reset()
{
  for (auto p : init_writes) {
    write_chunk(p.first, sizeof(uint32_t), &p.second);
  }
  if (write_hart0_msip)
    tsi_t::reset();
}
