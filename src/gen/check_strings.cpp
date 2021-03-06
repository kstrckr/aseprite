// Aseprite Code Generator
// Copyright (c) 2016 David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include "gen/check_strings.h"

#include "base/exception.h"
#include "base/file_handle.h"
#include "base/fs.h"
#include "base/split_string.h"
#include "base/string.h"
#include "base/unique_ptr.h"
#include "cfg/cfg.h"
#include "gen/common.h"

#include "tinyxml.h"

#include <cctype>
#include <iostream>
#include <stdexcept>
#include <vector>

typedef std::vector<TiXmlElement*> XmlElements;

static std::string find_first_id(TiXmlElement* elem)
{
  TiXmlElement* child = elem->FirstChildElement();
  while (child) {
    const char* id = child->Attribute("id");
    if (id)
      return id;

    std::string idStr = find_first_id(child);
    if (!idStr.empty())
      return idStr;

    child = child->NextSiblingElement();
  }
  return "";
}

static void collect_widgets_with_strings(TiXmlElement* elem, XmlElements& widgets)
{
  TiXmlElement* child = elem->FirstChildElement();
  while (child) {
    const char* text = child->Attribute("text");
    const char* tooltip = child->Attribute("tooltip");
    if (text || tooltip)
      widgets.push_back(child);
    collect_widgets_with_strings(child, widgets);
    child = child->NextSiblingElement();
  }
}

static bool has_alpha_char(const char* p)
{
  while (*p) {
    if (std::isalpha(*p))
      return true;
    else
      ++p;
  }
  return false;
}

static bool is_email(const char* p)
{
  if (!*p || !std::isalpha(*p))
    return false;
  ++p;

  while (*p && (std::isalpha(*p) || *p == '.'))
    ++p;

  if (*p != '@')
    return false;
  ++p;

  while (*p && (std::isalpha(*p) || *p == '.'))
    ++p;

  // Return true if we are in the end of string
  return (*p == 0);
}

class CheckStrings {
public:

  void loadStrings(const std::string& dir) {
    for (const auto& fn : base::list_files(dir)) {
      cfg::CfgFile* f = new cfg::CfgFile;
      f->load(base::join_path(dir, fn));
      m_stringFiles.push_back(f);
    }
  }

  void checkStringsOnWidgets(const std::string& dir) {
    for (const auto& fn : base::list_files(dir)) {
      std::string fullFn = base::join_path(dir, fn);
      base::FileHandle inputFile(base::open_file(fullFn, "rb"));
      base::UniquePtr<TiXmlDocument> doc(new TiXmlDocument());
      doc->SetValue(fullFn.c_str());
      if (!doc->LoadFile(inputFile.get())) {
        std::cerr << doc->Value() << ":"
                  << doc->ErrorRow() << ":"
                  << doc->ErrorCol() << ": "
                  << "error " << doc->ErrorId() << ": "
                  << doc->ErrorDesc() << "\n";

        throw std::runtime_error("invalid input file");
      }

      TiXmlHandle handle(doc);
      XmlElements widgets;

      m_prefixId = find_first_id(doc->RootElement());

      collect_widgets_with_strings(doc->RootElement(), widgets);
      for (TiXmlElement* elem : widgets) {
        checkString(elem, elem->Attribute("text"));
        checkString(elem, elem->Attribute("tooltip"));
      }
    }
  }

  void checkString(TiXmlElement* elem, const char* text) {
    if (!text)
      return;                   // Do nothing
    else if (text[0] == '@') {
      for (auto cfg : m_stringFiles) {
        std::string lang = base::get_file_title(cfg->filename());
        std::string section, var;

        if (text[1] == '.') {
          section = m_prefixId.c_str();
          var = text+2;
        }
        else {
          std::vector<std::string> parts;
          base::split_string(text, parts, ".");
          if (parts.size() >= 1) section = parts[0].c_str()+1;
          if (parts.size() >= 2) var = parts[1];
        }

        const char* translated =
          cfg->getValue(section.c_str(), var.c_str(), nullptr);
        if (!translated || translated[0] == 0) {
          std::cerr << elem->GetDocument()->Value() << ":"
                    << elem->Row() << ":"
                    << elem->Column() << ": "
                    << "warning: <" << lang
                    << "> translation for a string ID wasn't found '"
                    << text << "' (" << section << "." << var << ")\n";
        }
      }
    }
    else if (has_alpha_char(text) &&
             !is_email(text)) {
      std::cerr << elem->GetDocument()->Value() << ":"
                << elem->Row() << ":"
                << elem->Column() << ": "
                << "warning: raw string found '"
                << text << "'\n";
    }
  }

private:
  std::vector<cfg::CfgFile*> m_stringFiles;
  std::string m_prefixId;
};

void check_strings(const std::string& widgetsDir,
                   const std::string& stringsDir)
{
  CheckStrings cs;
  cs.loadStrings(stringsDir);
  cs.checkStringsOnWidgets(widgetsDir);
}
