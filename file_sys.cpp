// $Id: file_sys.cpp,v 1.5 2016-01-14 16:16:52-08 - - $
// Partner: Darius Sakhapour(dsakhapo@ucsc.edu)
// Partner: Ryan Wong (rystwong@ucsc.edu)

#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <iomanip>
using namespace std;

#include "debug.h"
#include "file_sys.h"
#include "commands.h"
int inode::next_inode_nr {1};

//        *********************************************
//        ************** Misc. Functions **************
//        *********************************************

struct file_type_hash {
   size_t operator() (file_type type) const {
      return static_cast<size_t> (type);
   }
};

ostream& operator<< (ostream& out, file_type type) {
   static unordered_map<file_type,string,file_type_hash> hash {
      {file_type::PLAIN_TYPE, "PLAIN_TYPE"},
      {file_type::DIRECTORY_TYPE, "DIRECTORY_TYPE"},
   };
   return out << hash[type];
}

ostream& operator<< (ostream& out, const inode_state& state) {
   out << "inode_state: root = " << state.root
       << ", cwd = " << state.cwd;
   return out;
}

file_error::file_error (const string& what):
            runtime_error (what) {
}

//        ***************************************************
//        ************** Inode State Functions **************
//        ***************************************************

// Default constructor for inode_state.
// After the constructor is called, the root directory is created here.
// Thus, the cwd and parent both refer to the root, since this new
// directory is the root and the root's parent is itself.
inode_state::inode_state() {
   root = make_shared<inode>(file_type::DIRECTORY_TYPE);
   cwd = root; parent = root;
   root->contents->set_dir(cwd, parent);
   root->set_name("/");
   DEBUGF ('i', "root = " << root << ", cwd = " << cwd
          << ", prompt = \"" << prompt() << "\"");
}

// Shows the prompt character in console.
const string& inode_state::prompt() { return prompt_; }

// Prints out the given directory name of the current directory.
void inode_state::print_path(const inode_ptr& curr_dir) const {
   vector<string> path;
   path.push_back(curr_dir->get_name());
   map<string, inode_ptr> dirents = curr_dir->contents->get_contents();
   // Given a specific directory, use each inode's parent pointer to
   // traverse backwards through the directory until you find the root.
   inode_ptr parent = dirents.at("..");
   while(parent->get_inode_nr() > 1){
      path.push_back(parent->get_name());
      map<string, inode_ptr> dirents = parent->contents->get_contents();
      parent = dirents.at("..");
   }
   // Print the path vector to create the current directory.
   for(auto i = path.cend() - 1; i != path.cbegin() - 1; --i){
      if(i == path.cend() - 1) cout << *i;
      else if(i > path.cbegin()) cout << *i << "/";
      else cout << *i;
   }
   cout << endl;
}

// Prints the directory after being called by ls and lsr.
// Pulls information from directory contents, and displays them in
// an orderly manner.
// Top line should show the directory name.
// The directory listings will show the inode number, directory/file
// size, and the name of the contents (directory or plain file) inside
// in that order.
void inode_state::print_directory(const inode_ptr& curr_dir,
         const wordvec& args) const {
   if (args.size() == 1) {
      cout << curr_dir->get_name() << ":" << endl;
      map<string, inode_ptr> dirents =
               curr_dir->contents->get_contents();
      for (auto i = dirents.cbegin(); i != dirents.cend(); ++i) {
         cout << setw(6) << i->second->get_inode_nr() << "  " << setw(6)
                  << i->second->contents->size() << "  " << i->first
                  << endl;
      }
   } else if (args.size() > 1) {
      wordvec path_name = split(args.at(1), "/");
      map<string, inode_ptr> dirents =
               curr_dir->contents->get_contents();
      // mk_dir will search for the directory that the new directory
      // will be created in.
      // dir_found isolates directories that return correct directory
      // locations. Used to navigate towards the given directory.
      inode_ptr mk_dir = curr_dir;
      bool dir_found = false;
      // Walks through the list of path names, comparing them against
      // the path names within the current directory.
      for (size_t i = 0; i < path_name.size(); ++i) {
         dir_found = false;
         for (auto j = dirents.cbegin(); j != dirents.cend(); ++j) {
            // If the given pathname matches one being walked through
            // in the current directory, move the mk_dir into the
            // that pathname.
            if (j->first == path_name.at(i) + "/") {
               mk_dir = j->second;
               dir_found = true;
            }
         }
         if (dir_found == false) {
            throw command_error("make_directory: invalid pathname");
         }
         // Once in the new directory, get its current contents.
         dirents = mk_dir->contents->get_contents();
         // This searching crawl method will be repeated until
         // either the given directory is made with the new
         // directory inside of it, or an error is found.
      }
      // new_dir is inode for the directory being made.
      // It is inserted into its own parent directory, since new_dir represents
      // the directory being created.

      // Fix for name. Moves the / from the end of the name to begin.
      string name_fix = mk_dir->get_name();
      name_fix.pop_back();
      name_fix = "/" + name_fix;

      cout << name_fix << ":" << endl;
      for (auto i = dirents.cbegin(); i != dirents.cend(); ++i) {
         cout << setw(6) << i->second->get_inode_nr() << "  " << setw(6)
                  << i->second->contents->size() << "  " << i->first
                  << endl;
      }
   } else {
      throw command_error("make_directory: invalid number of arguments");
   }
}

// Creates a new file for mkfile command, parses out the words to be
// included in the file itself, then sets the pointers to put the file
// within the current directory.
void inode_state::create_file
(const inode_ptr& curr_dir, const wordvec& words) const {
   inode_ptr file = curr_dir->contents->mkfile(words.at(1));
   vector<string> data;
   // If the mkfile command is meant to add words to the file.
   if (words.size() > 2) {
      // Start at 2, since the first two positions in the vector
      // point to the command function and the file name. Everything
      // after that are words to be added to the function.
      for (size_t i = 2; i < words.size(); ++i) {
         data.push_back(words.at(i));
      }
   }      // If no words in mkfile command, make an empty file.
   else
      data.push_back("");
   //Check to see if a dir or a file with the same name exists
   map<string, inode_ptr> dirents = curr_dir->contents->get_contents();
   for (auto i = dirents.cbegin(); i != dirents.cend(); ++i) {
      // If the file has the same name as a directory, throw an error.
      if (i->first == words.at(1) + "/") {
         throw command_error("make_directory: "
                  "directory has same name");
         // If the file has the same name as an existing file, replace
         // the existing file with the new one (including new data).
      } else if (i->first == words.at(1)) {
         file = i->second;
      }
   }
   file->contents->set_data(data);
   dirents.insert(pair<string, inode_ptr>(file->get_name(), file));
   curr_dir->contents->set_contents(dirents);
}

// Reads a plain file and outputs its text.
// Captures the current directory and its contents, scans each one to
// see if a content name matches the given search name, checks to make
// sure it is a readable file, and then outputs the file's word vector.
void inode_state::read_file(const inode_ptr& curr_dir,
         const wordvec& words) const {
   for (size_t k = 1; k != words.size(); ++k) {
      bool file_found = false;      // Flags true if file found.
      map<string, inode_ptr> dirents =
               curr_dir->contents->get_contents();
      for (auto i = dirents.cbegin(); i != dirents.cend(); ++i) {
         // Search to see if a file or directory shares the name.
         if (i->first == words.at(k)) {
            // See if the matching file is a directory.
            if (i->second->contents->is_dir() == false) {
               file_found = true;
               for (auto j = i->second->contents->readfile().begin();
                        j != i->second->contents->readfile().end();
                        ++j) {
                  cout << *j << " ";
               }
               // If the match is a directory, throw an error.
            } else if (i->second->contents->is_dir() == true) {
               throw command_error("fn_cat: cannot read directories.");
            }
            cout << endl;
         }
      }
      // If there are no matches in the directory's entities, error.
      if (!file_found) {
         throw command_error("fn_cat: file not found.");
      }
   }
}

// Makes a new directory. If called with a directory path, will create
// the new directory in that path. Otherwise, it will create the
// new directory in the current path.
void inode_state::make_directory(const inode_ptr& curr_dir,
         const wordvec& path) const {
   // Takes the pathname parameter and separates the given directory
   // into individual directory path names.
   wordvec path_name = split(path.at(1), "/");
   map<string, inode_ptr> dirents = curr_dir->contents->get_contents();
   // mk_dir will search for the directory that the new directory
   // will be created in.
   // dir_found isolates directories that return correct directory
   // locations. Used to navigate towards the given directory.
   inode_ptr mk_dir = curr_dir;
   bool dir_found = false;
   // Walks through the list of path names, comparing them against
   // the path names within the current directory.
   for (size_t i = 0; i < path_name.size() - 1; ++i) {
      dir_found = false;
      for (auto j = dirents.cbegin(); j != dirents.cend(); ++j) {
         // If the given pathname matches one being walked through
         // in the current directory, move the mk_dir into the
         // that pathname.
         if (j->first == path_name.at(i) + "/") {
            mk_dir = j->second;
            dir_found = true;
         }
      }
      if (dir_found == false) {
         throw command_error("make_directory: invalid pathname");
      }
      // Once in the new directory, get its current contents.
      dirents = mk_dir->contents->get_contents();
      // This searching crawl method will be repeated until
      // either the given directory is made with the new
      // directory inside of it, or an error is found.
   }
   // new_dir is inode for the directory being made.
   // It is inserted into its own parent directory, since new_dir represents
   // the directory being created.
   inode_ptr new_dir = mk_dir->contents->mkdir(
            path_name.at(path_name.size() - 1));
   new_dir->contents->set_dir(new_dir, mk_dir);
   dirents.insert(
            pair<string, inode_ptr>(new_dir->get_name(), new_dir));
   mk_dir->contents->set_contents(dirents);
}

//        *********************************************
//        ************** Inode Functions **************
//        *********************************************

// Default constructor for inode.
// When inode is called with a file_type parameter, one of those
// respective files (Plain_type or Directory_type) gets constructed.
inode::inode(file_type type): inode_nr (next_inode_nr++) {
   switch (type) {
      case file_type::PLAIN_TYPE:
           contents = make_shared<plain_file>();
           break;
      case file_type::DIRECTORY_TYPE:
           contents = make_shared<directory>();
           break;
   }
   DEBUGF ('i', "inode " << inode_nr << ", type = " << type);
}

// Move to header later?
int inode::get_inode_nr() const {
   DEBUGF ('i', "inode = " << inode_nr);
   return inode_nr;
}

//       ****************************************************
//       *************** Plain File Functions ***************
//       ****************************************************

// Displays size of plain text file.
// Counts each individual character within a file.
size_t plain_file::size() const {
   size_t size {0};
   size = data.size();     // Accounts for spaces removed by delimiter.
   for (auto word = data.begin();
             word != data.end();
             word++) {
       size += word->size();     // Counts the characters per word.
   }
   // Compensates for a supposed extra space accounted for by
   // size = data.size() above if there is at least one word in file.
   if (size > 1) size -= 1;
   DEBUGF ('i', "size = " << size);
   return size;
}

const wordvec& plain_file::readfile() const {
   DEBUGF ('i', data);
   return data;
}

void plain_file::writefile (const wordvec& words) {
   DEBUGF ('i', words);
}

void plain_file::remove (const string&) {
   throw file_error ("is a plain file");
}

inode_ptr plain_file::mkdir (const string&) {
   throw file_error ("is a plain file");
}

inode_ptr plain_file::mkfile (const string&) {
   throw file_error ("is a plain file");
}

void plain_file::set_dir(inode_ptr cwd, inode_ptr parent){
   throw file_error("is a plain file");
}

map<string, inode_ptr>& plain_file::get_contents(){
   throw file_error("is a plain file");
}

void plain_file::set_contents(const map<string, inode_ptr>&){
   throw file_error("is a plain file");
}

//        ***************************************************
//        *************** Directory Functions ***************
//        ***************************************************

// Default constructor for directory.
// Each directory has null pointers to itself (.) and its parent (..)
// by default upon creation. These are then set with directory::set_dir.
directory::directory() {
   dirents.insert(pair<string, inode_ptr>(".", nullptr));
   dirents.insert(pair<string, inode_ptr>("..", nullptr));
}

// Sets the pointers for a directory.
// The first line sets the . pointer to the directory itself, and the
// second line sets the .. pointer to the directory's parent.
void directory::set_dir(inode_ptr cwd, inode_ptr parent){
   map<string, inode_ptr>::iterator i = dirents.begin();
   i->second = cwd; ++i;
   i->second = parent;
}

// Move to header later?
map<string, inode_ptr>& directory::get_contents(){
   return dirents;
}

// Move to header later?
void directory::set_contents(const map<string, inode_ptr>& new_map){
   dirents = new_map;
}

// Counts the entities within a directory, and returns the size.
size_t directory::size() const {
   size_t size {0};
   size = dirents.size();
   DEBUGF ('i', "size = " << size);
   return size;
}

const wordvec& directory::readfile() const {
   throw file_error ("is a directory");
}

void directory::writefile (const wordvec&) {
   throw file_error ("is a directory");
}

void directory::set_data(const wordvec& d){
   throw file_error("is a directory");
}
void directory::remove (const string& filename) {
   DEBUGF ('i', filename);
}

// Makes a new default directory in the current directory.
inode_ptr directory::mkdir (const string& dirname) {
   inode_ptr new_dir = make_shared<inode>(file_type::DIRECTORY_TYPE);
   new_dir->set_name(dirname + "/");
   DEBUGF ('i', dirname);
   return new_dir;
}

// Makes a new text file pointing to the current directory.
inode_ptr directory::mkfile (const string& filename) {
   inode_ptr file = make_shared<inode>(file_type::PLAIN_TYPE);
   file->set_name(filename);
   DEBUGF ('i', filename);
   return file;
}

