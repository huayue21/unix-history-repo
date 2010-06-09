-- This file is generated by SWIG. Do *not* modify by hand.
--

with Interfaces.C.Strings;


package LLVM_link_time_Optimizer.Binding is

   LTO_H           : constant := 1;
   LTO_API_VERSION : constant := 3;

   function lto_get_version return  Interfaces.C.Strings.chars_ptr;

   function lto_get_error_message return  Interfaces.C.Strings.chars_ptr;

   function lto_module_is_object_file
     (path : in Interfaces.C.Strings.chars_ptr)
      return Interfaces.C.Extensions.bool;

   function lto_module_is_object_file_for_target
     (path                 : in Interfaces.C.Strings.chars_ptr;
      target_triple_prefix : in Interfaces.C.Strings.chars_ptr)
      return                 Interfaces.C.Extensions.bool;

   function lto_module_is_object_file_in_memory
     (mem    : access Interfaces.C.Extensions.void;
      length : in Interfaces.C.size_t)
      return   Interfaces.C.Extensions.bool;

   function lto_module_is_object_file_in_memory_for_target
     (mem                  : access Interfaces.C.Extensions.void;
      length               : in Interfaces.C.size_t;
      target_triple_prefix : in Interfaces.C.Strings.chars_ptr)
      return                 Interfaces.C.Extensions.bool;

   function lto_module_create
     (path : in Interfaces.C.Strings.chars_ptr)
      return LLVM_link_time_Optimizer.lto_module_t;

   function lto_module_create_from_memory
     (mem    : access Interfaces.C.Extensions.void;
      length : in Interfaces.C.size_t)
      return   LLVM_link_time_Optimizer.lto_module_t;

   procedure lto_module_dispose
     (the_mod : in LLVM_link_time_Optimizer.lto_module_t);

   function lto_module_get_target_triple
     (the_mod : in LLVM_link_time_Optimizer.lto_module_t)
      return    Interfaces.C.Strings.chars_ptr;

   function lto_module_get_num_symbols
     (the_mod : in LLVM_link_time_Optimizer.lto_module_t)
      return    Interfaces.C.unsigned;

   function lto_module_get_symbol_name
     (the_mod : in LLVM_link_time_Optimizer.lto_module_t;
      index   : in Interfaces.C.unsigned)
      return    Interfaces.C.Strings.chars_ptr;

   function lto_module_get_symbol_attribute
     (the_mod : in LLVM_link_time_Optimizer.lto_module_t;
      index   : in Interfaces.C.unsigned)
      return    LLVM_link_time_Optimizer.lto_symbol_attributes;

   function lto_codegen_create return  LLVM_link_time_Optimizer.lto_code_gen_t;

   procedure lto_codegen_dispose
     (arg_1 : in LLVM_link_time_Optimizer.lto_code_gen_t);

   function lto_codegen_add_module
     (cg      : in LLVM_link_time_Optimizer.lto_code_gen_t;
      the_mod : in LLVM_link_time_Optimizer.lto_module_t)
      return    Interfaces.C.Extensions.bool;

   function lto_codegen_set_debug_model
     (cg    : in LLVM_link_time_Optimizer.lto_code_gen_t;
      arg_1 : in LLVM_link_time_Optimizer.lto_debug_model)
      return  Interfaces.C.Extensions.bool;

   function lto_codegen_set_pic_model
     (cg    : in LLVM_link_time_Optimizer.lto_code_gen_t;
      arg_1 : in LLVM_link_time_Optimizer.lto_codegen_model)
      return  Interfaces.C.Extensions.bool;

   procedure lto_codegen_set_gcc_path
     (cg   : in LLVM_link_time_Optimizer.lto_code_gen_t;
      path : in Interfaces.C.Strings.chars_ptr);

   procedure lto_codegen_set_assembler_path
     (cg   : in LLVM_link_time_Optimizer.lto_code_gen_t;
      path : in Interfaces.C.Strings.chars_ptr);

   procedure lto_codegen_add_must_preserve_symbol
     (cg     : in LLVM_link_time_Optimizer.lto_code_gen_t;
      symbol : in Interfaces.C.Strings.chars_ptr);

   function lto_codegen_write_merged_modules
     (cg   : in LLVM_link_time_Optimizer.lto_code_gen_t;
      path : in Interfaces.C.Strings.chars_ptr)
      return Interfaces.C.Extensions.bool;

   function lto_codegen_compile
     (cg     : in LLVM_link_time_Optimizer.lto_code_gen_t;
      length : access Interfaces.C.size_t)
      return   access Interfaces.C.Extensions.void;

   procedure lto_codegen_debug_options
     (cg    : in LLVM_link_time_Optimizer.lto_code_gen_t;
      arg_1 : in Interfaces.C.Strings.chars_ptr);

   function llvm_create_optimizer return
     LLVM_link_time_Optimizer.llvm_lto_t;

   procedure llvm_destroy_optimizer
     (lto : in LLVM_link_time_Optimizer.llvm_lto_t);

   function llvm_read_object_file
     (lto            : in LLVM_link_time_Optimizer.llvm_lto_t;
      input_filename : in Interfaces.C.Strings.chars_ptr)
      return           LLVM_link_time_Optimizer.llvm_lto_status_t;

   function llvm_optimize_modules
     (lto             : in LLVM_link_time_Optimizer.llvm_lto_t;
      output_filename : in Interfaces.C.Strings.chars_ptr)
      return            LLVM_link_time_Optimizer.llvm_lto_status_t;

private

   pragma Import (C, lto_get_version, "Ada_lto_get_version");
   pragma Import (C, lto_get_error_message, "Ada_lto_get_error_message");
   pragma Import
     (C,
      lto_module_is_object_file,
      "Ada_lto_module_is_object_file");
   pragma Import
     (C,
      lto_module_is_object_file_for_target,
      "Ada_lto_module_is_object_file_for_target");
   pragma Import
     (C,
      lto_module_is_object_file_in_memory,
      "Ada_lto_module_is_object_file_in_memory");
   pragma Import
     (C,
      lto_module_is_object_file_in_memory_for_target,
      "Ada_lto_module_is_object_file_in_memory_for_target");
   pragma Import (C, lto_module_create, "Ada_lto_module_create");
   pragma Import
     (C,
      lto_module_create_from_memory,
      "Ada_lto_module_create_from_memory");
   pragma Import (C, lto_module_dispose, "Ada_lto_module_dispose");
   pragma Import
     (C,
      lto_module_get_target_triple,
      "Ada_lto_module_get_target_triple");
   pragma Import
     (C,
      lto_module_get_num_symbols,
      "Ada_lto_module_get_num_symbols");
   pragma Import
     (C,
      lto_module_get_symbol_name,
      "Ada_lto_module_get_symbol_name");
   pragma Import
     (C,
      lto_module_get_symbol_attribute,
      "Ada_lto_module_get_symbol_attribute");
   pragma Import (C, lto_codegen_create, "Ada_lto_codegen_create");
   pragma Import (C, lto_codegen_dispose, "Ada_lto_codegen_dispose");
   pragma Import (C, lto_codegen_add_module, "Ada_lto_codegen_add_module");
   pragma Import
     (C,
      lto_codegen_set_debug_model,
      "Ada_lto_codegen_set_debug_model");
   pragma Import
     (C,
      lto_codegen_set_pic_model,
      "Ada_lto_codegen_set_pic_model");
   pragma Import
     (C,
      lto_codegen_set_gcc_path,
      "Ada_lto_codegen_set_gcc_path");
   pragma Import
     (C,
      lto_codegen_set_assembler_path,
      "Ada_lto_codegen_set_assembler_path");
   pragma Import
     (C,
      lto_codegen_add_must_preserve_symbol,
      "Ada_lto_codegen_add_must_preserve_symbol");
   pragma Import
     (C,
      lto_codegen_write_merged_modules,
      "Ada_lto_codegen_write_merged_modules");
   pragma Import (C, lto_codegen_compile, "Ada_lto_codegen_compile");
   pragma Import
     (C,
      lto_codegen_debug_options,
      "Ada_lto_codegen_debug_options");
   pragma Import (C, llvm_create_optimizer, "Ada_llvm_create_optimizer");
   pragma Import (C, llvm_destroy_optimizer, "Ada_llvm_destroy_optimizer");
   pragma Import (C, llvm_read_object_file, "Ada_llvm_read_object_file");
   pragma Import (C, llvm_optimize_modules, "Ada_llvm_optimize_modules");

end LLVM_link_time_Optimizer.Binding;
