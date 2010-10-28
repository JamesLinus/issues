#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <utility>
#include <iterator>
#include <stdexcept>
#include <map>
#include <algorithm>
#include <string>

// platform headers
#include <unistd.h>

// DEBUG VISUALIZATION TOOL ONLY
void display_issues(std::vector<std::pair<int, std::string> > const & issues) {
   for( auto const & x : issues ) {
      std::cout << x.first << "\t" << x.second << '\n';
   }
   std::cout << '\n';
}


// PRODUCTION CODE STARTS HERE

auto remove_pending(std::string stat) -> std::string {
   typedef std::string::size_type size_type;
   if( 0 == stat.find("Pending")) {
      stat.erase(size_type{0}, size_type{8});
   }
   return stat;
}


auto remove_tentatively(std::string stat) -> std::string {
    typedef std::string::size_type size_type;
    if( 0 == stat.find("Tentatively") ) {
        stat.erase(size_type{0}, size_type{12});
    }
    return stat;
}


auto remove_qualifier(std::string const & stat) -> std::string {
    return remove_tentatively(remove_pending(stat));
}


auto find_file(std::string const & status) -> std::string {
    if( 0 == status.find("Tentatively") ) {
        return "lwg-active.html";
    }

    auto stat = remove_qualifier(status);
    return stat == "TC1"            ?  "lwg-defects.html"
         : stat == "CD1"            ?  "lwg-defects.html"
         : stat == "WP"             ?  "lwg-defects.html"
         : stat == "DR"             ?  "lwg-defects.html"
         : stat == "TRDec"          ?  "lwg-defects.html"
         : stat == "Dup"            ?  "lwg-closed.html"
         : stat == "NAD"            ?  "lwg-closed.html"
         : stat == "NAD Future"     ?  "lwg-closed.html"
         : stat == "NAD Editorial"  ?  "lwg-closed.html"
         : stat == "NAD Concepts"   ?  "lwg-closed.html"
         : stat == "Ready"          ?  "lwg-active.html"
         : stat == "Review"         ?  "lwg-active.html"
         : stat == "New"            ?  "lwg-active.html"
         : stat == "Open"           ?  "lwg-active.html"
         : throw std::runtime_error{"unknown status " + stat};
}


auto is_active(std::string const & stat) -> bool {
    return find_file(stat) == "lwg-active.html";
}


auto read_issues(std::istream& is) -> std::vector<std::pair<int, std::string> > {
   // parse all issues from the specified stream, 'is'.
   // Throws 'runtime_error' if *any* parse step fails
   //
   // We assume 'is' refers to a "toc" html document, for either the current or a previous issues list.
   // The TOC file consists of a sequence of HTML <tr> elements - each element is one issue/row in the table
   //    First we read the whole stream into a single 'string'
   //    Then we search the string for the first <tr> marker
   //       The first row is the title row and does not contain an issue.
   //       If cannt find the first row, we flag an error and exit
   //    Next we loop through the string, searching for <tr> markers to indicate the start of each issue
   //       We parse the issue number and status from each row, and append a record to the result vector
   //       If any parse fails, throw a runtime_error
   //    If debugging, display the results to 'cout'

   std::istreambuf_iterator<char> first{is}, last{};
   std::string s{first, last};

   // Skip the title row
   auto i = s.find("<tr>");
   if (std::string::npos == i) {
      throw std::runtime_error{"Unable to find the first (title) row"};
   }

   // Read all issues in table
   std::vector<std::pair<int, std::string> > issues;
   while (true) {
      i = s.find("<tr>", i+4);
      if (i == std::string::npos) {
         break;
      }
      i = s.find("</a>", i);
      auto j = s.rfind('>', i);
      if (j == std::string::npos) {
         throw std::runtime_error{"unable to parse issue number: can't find beginning bracket"};
      }
      std::istringstream instr{s.substr(j+1, i-j-1)};
      int num;
      instr >> num;
      if (instr.fail()) {
         throw std::runtime_error{"unable to parse issue number"};
      }
      i = s.find("</a>", i+4);
      if (i == std::string::npos) {
         throw std::runtime_error{"partial issue found"};
      }
      j = s.rfind('>', i);
      if (j == std::string::npos) {
         throw std::runtime_error{"unable to parse issue status: can't find beginning bracket"};
      }
      issues.push_back({num, s.substr(j+1, i-j-1)});
   }

   //display_issues(issues);
   return issues;
}


void list_issues(std::vector<int> const & issues ) {
   auto list_separator = "";
   for (auto number : issues ) {
      std::cout << list_separator << "<iref ref=\"" << number << "\"/>";
      list_separator = ", ";
   }
}


struct find_num {
   bool operator()(const std::pair<int, std::string>& x, int y) const noexcept {
      return x.first < y;
   }
};

void discover_new_issues( std::vector<std::pair<int, std::string>> const & old_issues
                        , std::vector<std::pair<int, std::string>> const & new_issues ) {

   std::map<std::string, std::vector<int> > added_issues;
   for( auto const & i : new_issues ) {
      auto j = std::lower_bound(old_issues.cbegin(), old_issues.cend(), i.first, find_num{});
      if(j == old_issues.end()) {
         added_issues[i.second].push_back(i.first);
      }
   }

   for( auto const & i : added_issues ) {
      auto const item_count = i.second.size();
      if(1 == item_count) {
         std::cout << "<li>Added the following " << i.first << " issue: <iref ref=\"" << i.second.front() << "\"/>.</li>\n";
      }
      else {
         std::cout << "<li>Added the following " << item_count << " " << i.first << " issues: ";
         list_issues(i.second);
         std::cout << ".</li>\n";
      }
   }
}


struct reverse_pair {
   // member template means we cannot make thi a local class
   template <class T, class U>
   auto operator()(const T& x, const U& y) const noexcept -> bool {
      return static_cast<bool>(  x.second < y.second  or
                              (!(y.second < x.second)  and  x.first < y.first));
   }
};

void discover_changed_issues(const std::vector<std::pair<int, std::string> >& old_issues,
                             const std::vector<std::pair<int, std::string> >& new_issues)
{
   std::map<std::pair<std::string, std::string>, std::vector<int>, reverse_pair> changed_issues;
   for (auto const & i : new_issues ) {
      auto j = std::lower_bound(old_issues.begin(), old_issues.end(), i.first, find_num{});
      if (j != old_issues.end()  and  i.first == j->first  and  j->second != i.second) {
         changed_issues[make_pair(j->second, i.second)].push_back(i.first);
      }
   }

   for (auto const & i : changed_issues ) {
      auto const item_count = i.second.size();
      if(1 == item_count) {
         std::cout << "<li>Changed the following issue from " << i.first.first << " to " << i.first.second
                   << ": <iref ref=\"" << i.second.front() << "\"/>.</li>\n";
      }
      else {
         std::cout << "<li>Changed the following " << item_count << " issues from " << i.first.first << " to " << i.first.second << ": ";
         list_issues(i.second);
         std::cout << ".</li>\n";
      }
   }
}


void count_issues(const std::vector<std::pair<int, std::string> >& issues, unsigned& n_open, unsigned& n_closed) {
   n_open = 0;
   n_closed = 0;

   for(auto const & elem : issues) {
      if (is_active(elem.second)) {
         ++n_open;
      }
      else {
         ++n_closed;
      }
   }
}


void write_summary(const std::vector<std::pair<int, std::string> >& old_issues,
                   const std::vector<std::pair<int, std::string> >& new_issues)
{
   unsigned n_open_new = 0;
   unsigned n_open_old = 0;
   unsigned n_closed_new = 0;
   unsigned n_closed_old = 0;
   count_issues(old_issues, n_open_old, n_closed_old);
   count_issues(new_issues, n_open_new, n_closed_new);

   std::cout << "<li>" << n_open_new << " open issues, ";
   if (n_open_new >= n_open_old) {
      std::cout << "up by " << n_open_new - n_open_old << ".</li>\n";
   }
   else {
      std::cout << "down by " << n_open_old - n_open_new << ".</li>\n";
   }

   std::cout << "<li>" << n_closed_new << " closed issues, ";
   if (n_closed_new >= n_closed_old) {
      std::cout << "up by " << n_closed_new - n_closed_old << ".</li>\n";
   }
   else {
      std::cout << "down by " << n_closed_old - n_closed_new << ".</li>\n";
   }

   unsigned n_total_new = n_open_new + n_closed_new;
   unsigned n_total_old = n_open_old + n_closed_old;
   std::cout << "<li>" << n_total_new << " issues total, ";
   if (n_total_new >= n_total_old) {
      std::cout << "up by " << n_total_new - n_total_old << ".</li>\n";
   }
   else {
      std::cout << "down by " << n_total_old - n_total_new << ".</li>\n";
   }
}


auto read_issues(std::string const & filename) -> std::vector<std::pair<int, std::string> > {
   std::ifstream new_html{filename.c_str()};
   if(!new_html.is_open()) {
      throw std::runtime_error{"Unable to open toc file: " + filename};
   }
   return read_issues(new_html);
}


int main (int argc, char* const argv[]) {
   try {
      std::string path;
      if(2 == argc) {
         path = argv[1];
      }
      else {
         char cwd[1024];
         if (getcwd(cwd, sizeof(cwd)) == 0) {
            std::cout << "unable to getcwd\n";
            return 1;
         }
         path = cwd;
      }

      if (path.back() != '/') { path += '/'; }

      auto const old_issues = read_issues(path + "meta-data/lwg-toc.old.html");
      auto const new_issues = read_issues(path + "lwg-toc.html");

      std::cout << "<ul>\n";
      std::cout << "<li><b>Summary:</b><ul>\n";
      write_summary(old_issues, new_issues);
      std::cout << "</ul></li>\n";
      std::cout << "<li><b>Details:</b><ul>\n";
      discover_new_issues(old_issues, new_issues);
      discover_changed_issues(old_issues, new_issues);
      std::cout << "</ul></li>\n";
      std::cout << "</ul>\n";
   }
   catch(std::exception const & ex) {
      std::cout << ex.what() << std::endl;
      return 1;
   }
}