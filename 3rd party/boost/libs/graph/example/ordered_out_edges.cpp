//=======================================================================
// Copyright 1997, 1998, 1999, 2000 University of Notre Dame.
// Authors: Andrew Lumsdaine, Lie-Quan Lee, Jeremy G. Siek
//
// This file is part of the Boost Graph Library
//
// You should have received a copy of the License Agreement for the
// Boost Graph Library along with the software; see the file LICENSE.
// If not, contact Office of Research, University of Notre Dame, Notre
// Dame, IN 46556.
//
// Permission to modify the code and to distribute modified code is
// granted, provided the text of this NOTICE is retained, a notice that
// the code was modified is included with the above COPYRIGHT NOTICE and
// with the COPYRIGHT NOTICE in the LICENSE file, and that the LICENSE
// file is distributed with the modified code.
//
// LICENSOR MAKES NO REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.
// By way of example, but not limitation, Licensor MAKES NO
// REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
// PARTICULAR PURPOSE OR THAT THE USE OF THE LICENSED SOFTWARE COMPONENTS
// OR DOCUMENTATION WILL NOT INFRINGE ANY PATENTS, COPYRIGHTS, TRADEMARKS
// OR OTHER RIGHTS.
//=======================================================================
#include <boost/config.hpp>
#include <iostream>
#include <functional>
#include <string>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/properties.hpp>

/*
  Sample output:

  0  --chandler--> 1   --joe--> 1  
  1  --chandler--> 0   --joe--> 0   --curly--> 2   --dick--> 3   --dick--> 3  
  2  --curly--> 1   --tom--> 4  
  3  --dick--> 1   --dick--> 1   --harry--> 4  
  4  --tom--> 2   --harry--> 3  

  name(0,1) = chandler

  name(0,1) = chandler
  name(0,1) = joe

 */

template <class StoredEdge>
struct order_by_name
  : public std::binary_function<StoredEdge,StoredEdge,bool> 
{
  bool operator()(const StoredEdge& e1, const StoredEdge& e2) const {
    // Order by target vertex, then by name. 
    // std::pair's operator< does a nice job of implementing
    // lexicographical compare on tuples.
    return std::make_pair(e1.get_target(), boost::get(boost::edge_name, e1))
      < std::make_pair(e2.get_target(), boost::get(boost::edge_name, e2));
  }
};

#if !defined BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION
struct ordered_set_by_nameS { };
namespace boost {
  template <class ValueType>
  struct container_gen<ordered_set_by_nameS, ValueType> {
    typedef std::multiset<ValueType, order_by_name<ValueType> > type;
  };
}
#else
struct ordered_set_by_nameS {
  template <class T>
  struct bind_ { typedef std::multiset<T, order_by_name<T> > type; };
};
namespace boost {
  template <> struct container_selector<ordered_set_by_nameS>  {
    typedef ordered_set_by_nameS type;
  };
}
#endif

namespace boost {
  template <>
  struct parallel_edge_traits<ordered_set_by_nameS> { 
    typedef allow_parallel_edge_tag type;
  };
}

int
main()
{
#ifdef BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION
  std::cout << "This program requires partial specialization" << std::endl;
#else
  using namespace boost;
  typedef property<edge_name_t, std::string> EdgeProperty;
  typedef adjacency_list<ordered_set_by_nameS, vecS, undirectedS,
    no_property, EdgeProperty> graph_type;
  graph_type g;
  
  add_edge(0, 1, EdgeProperty("joe"), g);
  add_edge(1, 2, EdgeProperty("curly"), g);
  add_edge(1, 3, EdgeProperty("dick"), g);
  add_edge(1, 3, EdgeProperty("dick"), g);
  add_edge(2, 4, EdgeProperty("tom"), g);
  add_edge(3, 4, EdgeProperty("harry"), g);
  add_edge(0, 1, EdgeProperty("chandler"), g);

  property_map<graph_type, vertex_index_t>::type id = get(vertex_index, g);
  property_map<graph_type, edge_name_t>::type name = get(edge_name, g);

  graph_traits<graph_type>::vertex_iterator i, end;
  graph_traits<graph_type>::out_edge_iterator ei, edge_end;
  for (boost::tie(i, end) = vertices(g); i != end; ++i) {
    std::cout << id[*i] << " ";
    for (boost::tie(ei, edge_end) = out_edges(*i, g); ei != edge_end; ++ei)
      std::cout << " --" << name[*ei] << "--> " << id[target(*ei, g)] << "  ";
    std::cout << std::endl;
  }
  std::cout << std::endl;

  bool found;
  typedef graph_traits<graph_type> Traits;
  Traits::edge_descriptor e;
  Traits::out_edge_iterator e_first, e_last;

  tie(e, found) = edge(0, 1, g);
  if (found)
    std::cout << "name(0,1) = " << name[e] << std::endl;
  else
    std::cout << "not found" << std::endl;
  std::cout << std::endl;

  tie(e_first, e_last) = edge_range(0, 1, g);
  while (e_first != e_last)
    std::cout << "name(0,1) = " << name[*e_first++] << std::endl;
#endif
  return 0;
}
