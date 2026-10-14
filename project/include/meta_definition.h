#ifndef META_DEFINITION
#define META_DEFINITION
#include "devcommon.h"
namespace ECProject
{
  enum OpperateType
  {
    SET,
    GET,
    DEL,
    REPAIR,
    MERGE,
    APPEND
  };
  enum EncodeType
  {
    Azure_LRC,
    Optimal_Cauchy_LRC
  };
  enum SingleStripePlacementType
  {
    Random,
    Flat,
    Optimal
  };
  enum MultiStripesPlacementType
  {
    Ran,
    DIS,
    AGG,
    OPT
  };

  typedef struct Block
  {
    int block_id;
    std::string block_key;
    char block_type;
    int block_size;
    int map2group, map2stripe, map2node;
    std::string map2key;
    Block(int block_id, const std::string &block_key, char block_type, int block_size,
          int map2group, int map2stripe, int map2node, const std::string &map2key)
        : block_id(block_id), block_key(block_key), block_type(block_type), block_size(block_size),
          map2group(map2group), map2stripe(map2stripe), map2node(map2node), map2key(map2key) {}
    Block() = default;
  } Block;

  typedef struct Node
  {
    int node_id;
    std::string proxy_ip;
    int proxy_port;
    std::string node_ip;
    int node_port;
    std::vector<Block *> blocks;
    std::unordered_map<int, int> stripes;
    Node(int id, const std::string &proxy_host, int proxy_service_port,
         const std::string &datanode_host, int datanode_service_port)
        : node_id(id), proxy_ip(proxy_host), proxy_port(proxy_service_port),
          node_ip(datanode_host), node_port(datanode_service_port) {}
    Node() = default;
  } Node;

  typedef struct Stripe
  {
    int k, l, g_m;
    int stripe_id;
    std::vector<std::string> object_keys;
    std::vector<int> object_sizes;
    std::vector<Block *> blocks;
    std::unordered_set<int> place2nodes;
    int n, r, z;
    int num_groups;
    std::map<int, std::vector<int>> group_to_blocks;
  } Stripe;

  typedef struct ObjectInfo
  {
    int object_size;
    int map2stripe;

    ObjectInfo() = default;
    ObjectInfo(int object_size, int map2stripe) : object_size(object_size), map2stripe(map2stripe) {}
  } ObjectInfo;

  typedef struct StripeOffset
  {
    int stripe_id;
    // range: [0, k*blockSize)
    // stripe organization
    int offset;

    StripeOffset() = default;
    StripeOffset(int stripe_id, int offset) : stripe_id(stripe_id), offset(offset) {}
  } StripeOffset;

  typedef struct ParitySlice
  {
    // physical offset
    int offset;
    int size;
    char *slice_ptr;

    ParitySlice() : offset(0), size(0), slice_ptr(nullptr) {}
    // the slice_ptr must be allocated on heap, otherwise will cause failure free
    explicit ParitySlice(int offset, int size, char *data)
        : offset(offset), size(size), slice_ptr(data) {}

    ~ParitySlice()
    {
      delete[] slice_ptr;
    }

    // 禁止复制构造函数和赋值运算符
    ParitySlice(const ParitySlice &) = delete;
    ParitySlice &operator=(const ParitySlice &) = delete;

    // 允许移动构造函数和赋值运算符
    ParitySlice(ParitySlice &&other) noexcept
        : offset(other.offset), size(other.size), slice_ptr(other.slice_ptr)
    {
      other.slice_ptr = nullptr;
    }

    ParitySlice &operator=(ParitySlice &&other) noexcept
    {
      if (this != &other)
      {
        delete[] slice_ptr;
        offset = other.offset;
        size = other.size;
        slice_ptr = other.slice_ptr;
        other.slice_ptr = nullptr;
      }
      return *this;
    }
  } ParitySlice;

  typedef struct ECSchema
  {
    ECSchema() = default;
    ECSchema(bool partial_decoding, EncodeType encodetype, SingleStripePlacementType s_stripe_placementtype,
             MultiStripesPlacementType m_stripe_placementtype, int k_datablock, int l_localparityblock, int g_m_globalparityblock,
             int b_datapergroup, int x_stripepermergegroup)
        : partial_decoding(partial_decoding), encodetype(encodetype), s_stripe_placementtype(s_stripe_placementtype),
          m_stripe_placementtype(m_stripe_placementtype), k_datablock(k_datablock), l_localparityblock(l_localparityblock),
          g_m_globalparityblock(g_m_globalparityblock), b_datapergroup(b_datapergroup), x_stripepermergegroup(x_stripepermergegroup) {}
    bool partial_decoding;
    EncodeType encodetype;
    SingleStripePlacementType s_stripe_placementtype;
    MultiStripesPlacementType m_stripe_placementtype;
    int k_datablock;
    int l_localparityblock;
    int g_m_globalparityblock;
    int b_datapergroup;
    int x_stripepermergegroup; // the product of xi
  } ECSchema;
} // namespace ECProject

#endif // META_DEFINITION