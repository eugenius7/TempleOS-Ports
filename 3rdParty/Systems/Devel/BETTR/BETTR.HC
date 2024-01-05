// Constants representing data types
#define TOML_VALUE_STRING 0
#define TOML_VALUE_INT 1
#define TOML_VALUE_FLOAT 2
#define TOML_VALUE_BOOL 3

// TOMLValue represents a key-value pair in a TOML file. With an added "type"
class TOMLValue {
  U8 * key;
  U8 * value;
  I32 type;
};

// TOMLSection represents a section in a TOML file.
class TOMLSection {
  U8 * name;
  TOMLValue * values;
  U32 value_count;
};

// TOMLFile represents the entire structure of a TOML file.
class TOMLFile {
  TOMLSection * sections;
  U32 section_count;
};

// Detect the type of a value. Types are I32
I32 DetectValueType(U8 * value_start) {
  if (!value_start) return -1;

  if ( * value_start == '\"') {
    return TOML_VALUE_STRING;
  } else if (StrFirstOcc(value_start, ".") != NULL) {
    return TOML_VALUE_FLOAT;
  } else if (StrCmp(value_start, "true") == 0 || StrCmp(value_start, "false") == 0) {
    return TOML_VALUE_BOOL;
  } else {
    return TOML_VALUE_INT;
  }
}

// ParseLine reads a single line of text from the TOML file and stores the key-value pair in the provided TOMLValue.
U0 ParseLine(U8 * line, TOMLValue * value) {
  if (!line || !value) return;

  MemSet(value, 0, sizeof(TOMLValue));
  U8 * equals = StrFirstOcc(line, "=");
  if (!equals) return;

  U8 * key_start = line;
  U8 * key_end = equals - 1;
  while (key_start < key_end && * key_start == ' ') {
    key_start++;
  }
  while (key_end > key_start && * key_end == ' ') {
    key_end--;
  }

  U8 * value_start = equals + 1;
  while ( * value_start == ' ') {
    value_start++;
  }

  U64 key_len = key_end - key_start + 1;
  U64 value_len = StrLen(value_start);

  value -> key = MAlloc(key_len + 1);
  if (!value -> key) return;
  MemCpy(value -> key, key_start, key_len);
  value -> key[key_len] = 0;

  // Detect the data type of the value
  value -> type = DetectValueType(value_start);

  if (value -> type == TOML_VALUE_STRING) {
    value_len -= 2; // Exclude the quotes from the length
    value_start++; // Exclude the first quote
  }
  value -> value = MAlloc(value_len + 1);
  if (!value -> value) {
    Free(value -> key);
    return;
  }
  MemCpy(value -> value, value_start, value_len);
  value -> value[value_len] = 0;
}

// ParseSection reads a line of text representing a section in the TOML file and stores the section name in the provided TOMLSection.
U0 ParseSection(U8 * line, TOMLSection * section) {
  MemSet(section, 0, sizeof(TOMLSection));

  U8 * section_start = line;
  while ( * section_start == ' ') { // Trim leading spaces
    section_start++;
  }

  if ( * section_start != '[') return; // Check for the opening bracket after trimming spaces

  U8 * section_end = StrFirstOcc(section_start, "]");
  if (!section_end) return;

  U64 length = section_end - section_start - 1;

  section -> name = MAlloc(length + 1);
  MemCpy(section -> name, section_start + 1, length);
  section -> name[length] = 0;
}

// ReadTOMLFile reads the contents of a TOML file and returns a TOMLFile structure containing the parsed data.
TOMLFile * ReadTOMLFile(U8 * file_path) {
  // Allocate and initialize a TOMLFile structure
  TOMLFile * toml_file = MAlloc(sizeof(TOMLFile));
  MemSet(toml_file, 0, sizeof(TOMLFile));
  toml_file -> section_count = 0;
  toml_file -> sections = 0;

  TOMLSection * current_section = 0;

  // Read the contents of the file into memory
  U64 file_size;
  U8 * file_data = FileRead(file_path, & file_size);

  if (file_data) {
    // Iterate through the file data line by line
    U8 * line_buf = file_data;
    U8 * file_end = file_data + file_size;
    while (line_buf < file_end) {
      // Find the end of the current line
      U8 * next_line = StrFirstOcc(line_buf, "\n");
      if (next_line) {
        * next_line = 0;
        next_line++;
      } else {
        next_line = file_end;
      }

      // Trim leading spaces from the current line
      U8 * trimmed_line = line_buf;
      while ( * trimmed_line == ' ') {
        trimmed_line++;
      }

      // Check if the line starts a new section
      if ( * trimmed_line == '[') {
        TOMLSection section;
        ParseSection(trimmed_line, & section);

        // Allocate a new section in the TOMLFile structure and store the parsed section
        TOMLSection * new_sections = MAlloc((toml_file -> section_count + 1) * sizeof(TOMLSection));
        if (toml_file -> sections) {
          MemCpy(new_sections, toml_file -> sections, toml_file -> section_count * sizeof(TOMLSection));
          Free(toml_file -> sections);
        }
        toml_file -> sections = new_sections;
        MemCpy( & toml_file -> sections[toml_file -> section_count], & section, sizeof(TOMLSection));
        toml_file -> section_count++;

        current_section = & toml_file -> sections[toml_file -> section_count - 1];
        current_section -> values = 0;
        current_section -> value_count = 0;
      } else if ( * trimmed_line != 0 && * trimmed_line != '\n') {
        // If the line is not a section, parse it as a key-value pair
        TOMLValue value;
        ParseLine(trimmed_line, & value);
        // Add the parsed key-value pair to the current section
        if (current_section) {
          TOMLValue * new_values = MAlloc((current_section -> value_count + 1) * sizeof(TOMLValue));
          if (current_section -> values) {
            MemCpy(new_values, current_section -> values, current_section -> value_count * sizeof(TOMLValue));
            Free(current_section -> values);
          }
          current_section -> values = new_values;
          MemCpy( & current_section -> values[current_section -> value_count], & value, sizeof(TOMLValue));
          current_section -> value_count++;
        }
      }

      // Move on to the next line
      line_buf = next_line;
    }

    // Free the file data
    Free(file_data);
  }

  return toml_file;
}

// FreeTOMLFile deallocates memory associated with a TOMLFile structure.
U0 FreeTOMLFile(TOMLFile * toml_file) {
  if (!toml_file) return;
  U64 section_idx, value_idx;
  for (section_idx = 0; section_idx < toml_file -> section_count; section_idx++) {
    TOMLSection * section = & toml_file -> sections[section_idx];
    if (section -> name) Free(section -> name);
    for (value_idx = 0; value_idx < section -> value_count; value_idx++) {
      TOMLValue * value = & section -> values[value_idx];
      if (value -> key) Free(value -> key);
      if (value -> value) Free(value -> value);
    }
    if (section -> values) Free(section -> values);
  }

  if (toml_file -> sections) Free(toml_file -> sections);
  Free(toml_file);
}

// FindTOMLSection finds a section in a TOML file by its name.
TOMLSection * FindTOMLSection(TOMLFile * toml_file, U8 * section_name) {
  if (!toml_file || !section_name) return NULL;
  U64 i;
  for (i = 0; i < toml_file -> section_count; i++) {
    if (StrCmp(toml_file -> sections[i].name, section_name) == 0) {
      return & toml_file -> sections[i];
    }
  }

  return NULL;
}

// FindTOMLValue finds a value in a TOML section by its key.
TOMLValue * FindTOMLValue(TOMLSection * section, U8 * key) {
  if (!section || !key) return NULL;
  U64 i;
  for (i = 0; i < section -> value_count; i++) {
    if (StrCmp(section -> values[i].key, key) == 0) {
      return & section -> values[i];
    }
  }

  return NULL;
}

// GetValue returns the value of a key-value pair within a section in a TOMLFile by its key.
// Note: This value is a string, hence why there is a heavy focus on typecasting below.
U8 * GetValue(TOMLFile * toml_file, U8 * section_name, U8 * key) {
  TOMLSection * section = FindTOMLSection(toml_file, section_name);
  if (section) {
    TOMLValue * value = FindTOMLValue(section, key);
    if (value) {
      return value -> value;
    }
  }
  return NULL;
}

// Typecasting to I64
I64 GetIntValue(TOMLFile * toml_file, U8 * section_name, U8 * key) {
  U8 * val = GetValue(toml_file, section_name, key);
  if (val) {
    return Str2I64(val);
  }
  return 0;
}

F64 GetFloatValue(TOMLFile * toml_file, U8 * section_name, U8 * key) {
  U8 * val = GetValue(toml_file, section_name, key);
  if (val) {
    return Str2F64(val);
  }
  return 0.0;
}
Bool GetBoolValue(TOMLFile * toml_file, U8 * section_name, U8 * key) {
  U8 * val = GetValue(toml_file, section_name, key);
  if (val) {
    if (StrCmp(val, "true") == 0) {
      return 1;
    } else {
      return 0;
    }
  }
  return 0;
}

U8 * TypeToString(I32 type) {
  switch (type) {
  case TOML_VALUE_STRING:
    return "string";
  case TOML_VALUE_INT:
    return "integer";
  case TOML_VALUE_FLOAT:
    return "float";
  case TOML_VALUE_BOOL:
    return "boolean";
  default:
    return "unknown";
  }
}

// PrintTOMLContent prints the content of a TOML file.
U0 PrintTOMLContent(TOMLFile * toml_file) {
  if (!toml_file) return;
  U64 section_idx, value_idx;
  for (section_idx = 0; section_idx < toml_file -> section_count; section_idx++) {
    TOMLSection * section = & toml_file -> sections[section_idx];
    "[%s]\n", section -> name;

    for (value_idx = 0; value_idx < section -> value_count; value_idx++) {
      TOMLValue * value = & section -> values[value_idx];
      "%s = %s (%s)\n", value -> key, value -> value, TypeToString(value -> type);
    }

    "\n";
  }
}

U0 Bettr() {
  // Replace "path/to/your/file.toml" with the actual path to the TOML file you want to read.
  TOMLFile * toml_file = ReadTOMLFile("Test.toml");
  if (toml_file) {
    PrintTOMLContent(toml_file);
    FreeTOMLFile(toml_file);
  } else {
    Print("Failed to read TOML file.\n");
  }
}