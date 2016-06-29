#ifndef AMD_COMPILER_DRIVER_H
#define AMD_COMPILER_DRIVER_H

#include <string>
#include <vector>
#include <cassert>

namespace amd {

/*
 * DataType is a kind of input, output or intermediate represenation 
 * to the compiler.
 */
enum DataType {
  DT_CL,
  DT_CL_HEADER,
  DT_LLVM_BC,
  DT_LLVM_LL,
  DT_EXECUTABLE,
  DT_INTERNAL,
};

class FileReference;
class File;
class Compiler;

/*
 * Data is a container for input, output or intermediate representation.
 *
 * id is used for DT_CL_HEADER to specify the header name.
 */
class Data {
private:
  std::string id;
  DataType type;

public:
  Data(DataType type_, const std::string& id_ = "")
    : id(id_), type(type_) {}
  virtual ~Data() {}
  DataType Type() const { return type; }
  const std::string& Id() const { return id; }
  virtual bool IsReadOnly() const = 0;
  virtual FileReference* ToInputFile(Compiler* comp, File *parent) = 0;
  virtual File* ToOutputFile(Compiler* comp, File *parent) = 0;
  virtual bool ReadOutputFile(File* f) = 0;
};

bool FileExists(const std::string& name);

/*
 * FileReference is a reference to some file.
 */
class FileReference : public Data {
private:
  std::string name;

public:
  FileReference(DataType type, const std::string& name_)
    : Data(type),
      name(name_) {}

  bool IsReadOnly() const override { return true; }
  const std::string& Name() const { return name; }
  FileReference* ToInputFile(Compiler* comp, File *parent) override { return this; }
  File* ToOutputFile(Compiler* comp, File *parent) override { assert(false); return 0; }
  bool ReadOutputFile(File* f) override { assert(false); return false; }
  bool ReadToString(std::string& s);
  bool Exists() const;
};

/*
 * File is a file that can also be modified.
 */
class File : public FileReference {
public:
  File(DataType type, const std::string& name)
    : FileReference(type, name) {}

  bool IsReadOnly() const override { return false; }
  File* ToOutputFile(Compiler* comp, File *parent) override { return this; }
  bool ReadOutputFile(File* f) override { assert(this == f); return true; }
  bool WriteData(const char* ptr, size_t size);
  bool Exists() const;
};

/*
 * BufferReference provides read-only  access to user-managed buffer.
 */
class BufferReference : public Data {
private:
  const char* ptr;
  size_t size;

public:
  BufferReference(DataType type, const char* ptr_, size_t size_, const std::string& id)
    : Data(type, id),
      ptr(ptr_), size(size_) {}

  bool IsReadOnly() const override { return true; }
  const char* Ptr() const { return ptr; }
  size_t Size() const { return size; }
  FileReference* ToInputFile(Compiler* comp, File *parent) override;
  File* ToOutputFile(Compiler* comp, File *parent) override;
  bool ReadOutputFile(File* f) override { assert(false); return false; }
};

/*
 * Buffer is a modifiable buffer backed by its own storage.
 */
class Buffer : public Data {
private:
  std::vector<char> buf;

public:
  Buffer(DataType type)
    : Data(type) {}

  bool IsReadOnly() const override { return false; }
  std::vector<char>& Buf() { return buf; }
  const std::vector<char>& Buf() const { return buf; }
  size_t Size() const { return buf.size(); }
  bool IsEmpty() const { return buf.size() == 0; }
  FileReference* ToInputFile(Compiler* comp, File *parent) override;
  File* ToOutputFile(Compiler* comp, File *parent) override;
  bool ReadOutputFile(File* f) override;
};

/*
 * Compiler may be used to invoke different phases OpenCL compiler.
 *
 * All Data instances and corresponding resources created by Compiler instance,
 * including files * and buffers are destroyed this compiler instance is destroyed.
 * Additionally, as debug information may contain references to intermediate files.
 *
 * The lifetime of Compiler instance should be normally same as lifetime
 * of OpenCL program that invokes it.
 *
 * Compiler is not guaranteed to be thread safe.
 */
class Compiler {
public:
  virtual ~Compiler() {}

  /*
   * Return output of this compiler.
   */
  virtual std::string Output() = 0;

  /*
   * Create new FileReference with given type and pointing to file with given name.
   *
   * If parent is specified, name is assumed to be relative to parent, otherwise absolute.
   */
  virtual FileReference* NewFileReference(DataType type, const std::string& name, File* parent = 0) = 0;

  /*
   * Create new FileReference with given type and pointing to file with given name.
   *
   * If parent is specified, name is assumed to be relative to parent, otherwise absolute.
   *
   * The file will not be automatically deleted when Compiler instance is destroyed.
   */
  virtual File* NewFile(DataType type, const std::string& name, File* parent = 0) = 0;

  /*
   * Create File* pointing to new temporary file name.
   *
   * If name is specified, it is used as temporary file name.
   *
   * If parent is specified, name is assumed to be relative to parent. Otherwise, it is
   * under temporary directory associated with this Compiler instance.
   *
   * The file will be automatically deleted when Compiler instance is destroyed.
   */
  virtual File* NewTempFile(DataType type, const std::string& name = "", File* parent = 0) = 0;

  /*
   * Create File* pointing to new temporary directory.
   *
   * If parent is specified, name is assumed to be relative to parent. Otherwise, it is
   * under temporary directory associated with this Compiler instance.
   *
   * The directory will be automatically deleted when Compiler instance is destroyed.
   */
  virtual File* NewTempDir(File* parent = 0) = 0;

  /*
   * Create BufferReference.
   */
  virtual BufferReference* NewBufferReference(DataType type, const char* ptr, size_t size, const std::string& id = "") = 0;

  /*
   * Create new Buffer (initially empty).
   */
  virtual Buffer* NewBuffer(DataType type) = 0;

  /*
   * Compile several inputs to LLVM Bitcode.
   *
   * Each input should have one of types DT_CL, DT_CL_HEADER, DT_LLVM_BC, DT_LLVM_LL.
   * output should have one of types DT_LLVM_BC or DT_LLVM_LL.
   *
   * Returns true on success or false on failure.
   */
  virtual bool CompileToLLVMBitcode(const std::vector<Data*>& inputs, Data* output, const std::vector<std::string>& options) = 0;

  /*
   * Link several LLVM Bitcode inputs.
   *
   * Each input should have one of types DT_LLVM_BC, DT_LLVM_LL.
   * output should have one of types DT_LLVM_BC, DT_LLVM_LL
   *
   * Returns true on success or false on failure.
   */
  virtual bool LinkLLVMBitcode(const std::vector<Data*>& inputs, Data* output, const std::vector<std::string>& options) = 0;

  /*
   * Compile several inputs directly to executable object.

   * Each input should have one of types DT_CL, DT_CL_HEADER, DT_LLVM_BC, DT_LLVM_LL.
   * output should have type DT_EXECUTABLE.
   *
   * Returns true on success or false on failure.
   */
  virtual bool CompileAndLinkExecutable(const std::vector<Data*>& inputs, Data* output, const std::vector<std::string>& options) = 0;
};

/*
 * CompilerFactory is used to create Compiler's.
 *
 * Normally there is single instance of CompilerFactory.
 *
 * CompilerFactory does not own Compiler's that it creates. They should be
 * destroyed with delete.
 */
class CompilerFactory {
public:
  /*
   * Create new instance of OpenCL compiler with AMDGPU backend.
   */
  Compiler* CreateAMDGPUCompiler(const std::string& llvmBin);
};

}

#endif // AMD_COMPILER_DRIVER_H
