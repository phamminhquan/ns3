#include <random>
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/stats-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

// ===============================
// This main.cc is used to generate
// a network topology based on config
// file
// ===============================

// MAIN
int main (int argc, char *argv[]) {
  // Hyperparameters
  std::cout << "=============== HYPERPARAMTERS ===============" << std::endl << std::endl;
  std::cout << "Setting Hyperparameters" << std::endl;
  StringValue config_filepath ("../config_files/line3.cfg");
  uint32_t packetSize = 256;
  Time sim_start ("1s");
  Time sim_duration ("2s");
  Time stop_time ("5s");
  StringValue channel_cap ("10Mbps");
  StringValue channel_delay ("1ms");
  double bernoulli_param = 0.5;

  std::cout << "Packet size: " << packetSize << " bytes" << std::endl;
  std::cout << "Channel capacity: " << channel_cap.Get() << std::endl;
  std::cout << "Channel delay: " << channel_delay.Get() << std::endl;
  std::cout << "Traffic probability: " << bernoulli_param << std::endl;
  std::cout << "Simulation start time (respect to 0s): " << sim_start.GetSeconds() << "s" << std::endl;
  std::cout << "Simulation duration (repsect to 0s): " << sim_duration.GetSeconds() << "s" << std::endl;
  std::cout << "Simulation stop time (respect to 0s): " << stop_time.GetSeconds() << "s" << std::endl;

  // Initialize ASCII helper
  AsciiTraceHelper asciiTraceHelper;

  // Parse config file
  std::cout << std::endl << "=============== PARSE CONFIG FILE ==============" << std::endl << std::endl;
  // Get file
  std::cout << "Get file from path: " << config_filepath.Get() << std::endl;
  std::ifstream file(config_filepath.Get());

  if (!file.is_open()) {
    std::cerr << "Couldn't open config file for reading." << std::endl;
    return 0;
  }

  // first line is number of nodes
  std::string num_nodes_str;
  std::getline(file, num_nodes_str);
  uint32_t num_nodes = std::stoi(num_nodes_str);
  std::cout << "Number of nodes: " << num_nodes << std::endl;

  // second line is number edges
  std::string num_edges_str;
  std::getline(file, num_edges_str);
  uint32_t num_edges = std::stoi(num_edges_str);
  std::cout << "Number of edges: " << num_edges << std::endl;
  
  // all other lines are connections
  uint32_t connections[num_edges][2];
  uint32_t i=0;
  std::string line;
  while (std::getline(file, line, '-')) {
    std::string node1 = line;
    std::string node2;
    std::getline(file, node2);
    connections[i][0] = std::stoi(node1);
    connections[i][1] = std::stoi(node2);
    std::cout << "Connection: " << connections[i][0] << "-" << connections[i][1] << std::endl;
    i++;
  }
  
  // Set precision for cout
  std::cout << std::endl << "=============== SETUP ===============" << std::endl << std::endl;
  std::cout << std::fixed;
  std::cout << std::setprecision(6);

  // Initialize bernoulli generator
  std::cout << "Initialize traffic probability as random device/generator" << std::endl;
  std::random_device r;
  std::default_random_engine generator{r()};
  std::bernoulli_distribution bernoulli_distribution(bernoulli_param);

  // Create nodes for subnetwork 1
  std::cout << "Create nodes" << std::endl;
  NodeContainer nw;
  nw.Create(num_nodes);

  // Create point to point link
  std::cout << "Create link" << std::endl;
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", channel_cap);
  p2p.SetChannelAttribute("Delay", channel_delay);

  // Instantiate internet stack on all nodes
  std::cout << "Install internet stack" << std::endl;
  InternetStackHelper stack;
  stack.InstallAll ();

  // Initialize IPV4 address and sink interfaces
  std::cout << "Create IPV4 interfaces" << std::endl;
  Ipv4AddressHelper addr;
  addr.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  Ipv4InterfaceContainer sink_interfaces[num_nodes];

  // Loop to create flows
  for (uint32_t i=0; i<num_edges; i++) {
    std::cout << "Create interface: " << connections[i][0] << "-" << connections[i][1] << std::endl;
    NetDeviceContainer dev = p2p.Install(nw.Get(connections[i][0]), nw.Get(connections[i][1]));
    addr.NewNetwork();
    interfaces = addr.Assign(dev);
    sink_interfaces[connections[i][0]].Add(interfaces.Get(0));
    sink_interfaces[connections[i][1]].Add(interfaces.Get(1));
  }
  //sink_interfaces.Add(interfaces.Get(1));

  // Initialize global routing table
  std::cout << "Initialize routing table" << std::endl;
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Initialize sink port and sink helpers
  std::cout << "Initialize source and sink helpers" << std::endl;
  uint16_t port = 8080;
  Address anyAddress (InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", anyAddress);
  ApplicationContainer sinkApp;

  // Initialize traffic helper
  std::cout << "Initialize traffic helper" << std::endl;
  OnOffHelper onoff = OnOffHelper("ns3::TcpSocketFactory",
                                  InetSocketAddress(sink_interfaces[0].GetAddress(0, 0), port));
  onoff.SetConstantRate(DataRate("1Mbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

  // Create traffic
  std::cout << "Create traffic" << std::endl;
  Ptr<OutputStreamWrapper> TrafficStream = asciiTraceHelper.CreateFileStream("traffic.log");
  for (uint32_t i=0; i<num_nodes; i++) {
    // Create source app with sink at node i
    std::cout << "Create source app at sink node: " << i << std::endl;
    AddressValue remoteAddr (InetSocketAddress (sink_interfaces[i].GetAddress(0, 0), port));
    onoff.SetAttribute("Remote", remoteAddr);

    // Install this app to all other node
    ApplicationContainer sourceApp;
    for (uint32_t j=0; j<num_nodes; j++) {
      if (i!=j) {   // same node
        // Implement Bernoulli RV to create traffic
        if (bernoulli_distribution(generator) == 1) {
          // Log to cout and traffic log file
          std::cout << "Bernoulli Success" << std::endl;
          *TrafficStream->GetStream() << j << "-" << i << std::endl;
          // Create source app
          std::cout << "Install source app on source node: " << j << std::endl;
          sourceApp.Add(onoff.Install(nw.Get(j)));
        }
      }
    }
    sourceApp.Start(sim_start);
    sourceApp.Stop(sim_duration);

    // :Create sink app
    std::cout << "Create and install sink app" << std::endl;
    sinkApp.Add(sinkHelper.Install(nw.Get(i)));
  }
  sinkApp.Start(sim_start);
  sinkApp.Stop(sim_duration);

  std::cout << std::endl << "============== OUTPUT ==============" << std::endl << std::endl;
  
  // RUN SIM
  // Run and stop
  Simulator::Stop(stop_time);
  Simulator::Run();
  Simulator::Destroy();
 
  // Print out number of packets received by each node
  for (uint32_t i=0; i<sinkApp.GetN(); i++) {
    Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get(i));
    std::cout << "Total Bytes Received by Node " << i << ": " << sink->GetTotalRx() << std::endl;
    double tp = sink->GetTotalRx() * 8 / 1000000 / (sim_duration.GetSeconds()-1);
    std::cout << "Throughput on Node " << i << ": " << tp << " Mbps" << std::endl;
  }
 
  return 0;
}
