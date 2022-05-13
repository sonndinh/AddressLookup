#include <ace/INET_Addr.h>
#include <ace/OS_NS_netdb.h>
#include <ace/Sock_Connect.h>
#include <ace/Log_Msg.h>

#include <string>
#include <cstring>

union ip46 {
  sockaddr_in  in4_;
#ifdef ACE_HAS_IPV6
  sockaddr_in6 in6_;
#endif /* ACE_HAS_IPV6 */
};

void print_addr(const ACE_INET_Addr& addr, const char* str) {
  ACE_TCHAR buffer[256];
  if (addr.addr_to_string(buffer, sizeof buffer) != 0) {
    ACE_ERROR((LM_ERROR, "ERROR: print_addr: Failed to convert address to string\n"));
  } else {
    ACE_DEBUG((LM_DEBUG, "DEBUG: print_addr: %C %C\n", str, buffer));
  }
}

void hostname_to_ip(std::string address) {
  ACE_DEBUG((LM_DEBUG, "DEBUG: hostname_to_ip: Resolving IP addresses from hostname %C\n", address.c_str()));
  std::string host_name_str;
  unsigned short port_number = 0;

#ifdef ACE_HAS_IPV6
  const std::string::size_type openb = address.find_first_of('[');
  const std::string::size_type closeb = address.find_first_of(']', openb);
  const std::string::size_type last_double = address.rfind("::", closeb);
  const std::string::size_type port_div = closeb != std::string::npos ?
    address.find_first_of(':', closeb + 1u) :
    (last_double != std::string::npos ?
     address.find_first_of(':', last_double + 2u) :
     address.find_last_of(':'));
#else
  const std::string::size_type port_div = address.find_last_of(':');
#endif

  if (port_div != std::string::npos) {
#ifdef ACE_HAS_IPV6
    if (openb != std::string::npos && closeb != std::string::npos) {
      host_name_str = address.substr(openb + 1u, closeb - 1u - openb);
    } else
#endif /* ACE_HAS_IPV6 */
    {
      host_name_str = address.substr(0, port_div);
    }
    port_number = static_cast<unsigned short>(std::strtoul(address.substr(port_div + 1u).c_str(), 0, 10));
  } else {
#ifdef ACE_HAS_IPV6
    if (openb != std::string::npos && closeb != std::string::npos) {
      host_name_str = address.substr(openb + 1u, closeb - 1u - openb);
    } else
#endif /* ACE_HAS_IPV6 */
    {
      host_name_str = address;
    }
  }

  const char* host_name = host_name_str.c_str();

  addrinfo hints;
  std::memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;

  // The ai_flags used to contain AI_ADDRCONFIG as well but that prevented
  // lookups from completing if there is no, or only a loopback, IPv6
  // interface configured. See Bugzilla 4211 for more info.

#if defined ACE_HAS_IPV6 && !defined IPV6_V6ONLY
  hints.ai_flags |= AI_V4MAPPED;
#endif

#if defined ACE_HAS_IPV6 && defined AI_ALL
  // Without AI_ALL, Windows machines exhibit inconsistent behaviors on
  // difference machines we have tested.
  hints.ai_flags |= AI_ALL;
#endif

  // Note - specify the socktype here to avoid getting multiple entries
  // returned with the same address for different socket types or
  // protocols. If this causes a problem for some reason (an address that's
  // available for TCP but not UDP, or vice-versa) this will need to change
  // back to unrestricted hints and weed out the duplicate addresses by
  // searching this->inet_addrs_ which would slow things down.
  hints.ai_socktype = SOCK_STREAM;

  addrinfo *res = 0;
  const int error = ACE_OS::getaddrinfo(host_name, 0, &hints, &res);
  if (error) {
    ACE_ERROR((LM_ERROR, "hostname_to_ip: Call to getaddrinfo() for hostname %C returned error: %d\n", host_name, error));
    return;
  }

  for (addrinfo* curr = res; curr; curr = curr->ai_next) {
    if (curr->ai_family != AF_INET && curr->ai_family != AF_INET6) {
      ACE_DEBUG((LM_DEBUG, "hostname_to_ip: Encounter an address that is not AF_INET or AF_INET6\n"));
      continue;
    }

    ACE_DEBUG((LM_DEBUG, "hostname_to_ip: setting ip46...\n"));
    ip46 addr;
    std::memset(&addr, 0, sizeof addr);
    ACE_DEBUG((LM_DEBUG, "hostname_to_ip: finished memset ip46...\n"));
    std::memcpy(&addr, curr->ai_addr, curr->ai_addrlen);
    ACE_DEBUG((LM_DEBUG, "hostname_to_ip: finished memcpy ip46...\n"));
#ifdef ACE_HAS_IPV6
    if (curr->ai_family == AF_INET6) {
      addr.in6_.sin6_port = ACE_NTOHS(port_number);
      ACE_DEBUG((LM_DEBUG, "hostname_to_ip: finished setting IPv6 port number to ip46...\n"));
    } else {
#endif /* ACE_HAS_IPV6 */
      addr.in4_.sin_port = ACE_NTOHS(port_number);
      ACE_DEBUG((LM_DEBUG, "hostname_to_ip: finished setting IPv4 port number to ip46...\n"));
#ifdef ACE_HAS_IPV6
    }
#endif /* ACE_HAS_IPV6 */

    ACE_INET_Addr temp;
    temp.set_addr(&addr, sizeof addr);
    ACE_DEBUG((LM_DEBUG, "hostname_to_ip: finished setting ACE_INET_Addr addr...\n"));
    temp.set_port_number(port_number, 1 /*encode*/);
    ACE_DEBUG((LM_DEBUG, "hostname_to_ip: finished setting ACE_INET_Addr port number...\n"));

    print_addr(temp, "==== IP address:");
  }
  //ACE_DEBUG((LM_DEBUG, "hostname_to_ip: start freeing res\n"));
  //ACE_OS::freeaddrinfo(res);
  //ACE_DEBUG((LM_DEBUG, "hostname_to_ip: finish freeing res\n"));
}

void address_info() {
  size_t addr_count;
  ACE_INET_Addr *addr_array = 0;
  const int result = ACE::get_ip_interfaces(addr_count, addr_array);
  if (result != 0 || addr_count < 1) {
    ACE_ERROR((LM_ERROR, "ERROR: print_address_info: ACE::get_ip_interfaces: Unable to probe network interfaces\n"));
    return;
  }

  ACE_DEBUG((LM_DEBUG, "address_info: There are %d interfaces\n", addr_count));
  for (size_t i = 0; i < addr_count; ++i) {
    ACE_DEBUG((LM_DEBUG, "address_info: Considering interface %d\n", i));
    ACE_TCHAR buffer[256];
    if (addr_array[i].addr_to_string(buffer, sizeof buffer) != 0) {
      ACE_ERROR((LM_ERROR, "ERROR: address_info: Failed to convert address to string\n"));
    } else {
      ACE_DEBUG((LM_DEBUG, "DEBUG: address_info: Found IP interface %C\n", buffer));
    }

    // Find the hostname of the interface
    char hostname[MAXHOSTNAMELEN+1] = "";
    if (ACE::get_fqdn(addr_array[i], hostname, MAXHOSTNAMELEN+1) == 0) {
      ACE_DEBUG((LM_DEBUG, "DEBUG: address_info: IP address %C maps to hostname %C\n", buffer, hostname));
    } else {
      ACE_ERROR((LM_ERROR, "ERROR: address_info: Failed to get FQDN\n"));
    }

    // Resolve the hostname back to a list of IP addreses
    hostname_to_ip(hostname);
    ACE_DEBUG((LM_DEBUG, "\n"));
  }
  ACE_DEBUG((LM_DEBUG, "\n"));
}

int main(int argc, char* argv[]) {
  ACE_DEBUG((LM_DEBUG, "========= Attempt 1....\n"));
  address_info();
  ACE_DEBUG((LM_DEBUG, "========= Attempt 2....\n"));
  address_info();
  return 0;
}
