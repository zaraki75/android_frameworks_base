/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AAPT2_LINK_H
#define AAPT2_LINK_H

#include "Command.h"
#include "Diagnostics.h"
#include "Resource.h"
#include "split/TableSplitter.h"
#include "format/binary/TableFlattener.h"
#include "link/ManifestFixer.h"

namespace aapt {

enum class OutputFormat {
  kApk,
  kProto,
};

struct LinkOptions {
  std::string output_path;
  std::string manifest_path;
  std::vector<std::string> include_paths;
  std::vector<std::string> overlay_files;
  std::vector<std::string> assets_dirs;
  bool output_to_directory = false;
  bool auto_add_overlay = false;
  OutputFormat output_format = OutputFormat::kApk;

  // Java/Proguard options.
  Maybe<std::string> generate_java_class_path;
  Maybe<std::string> custom_java_package;
  std::set<std::string> extra_java_packages;
  Maybe<std::string> generate_text_symbols_path;
  Maybe<std::string> generate_proguard_rules_path;
  Maybe<std::string> generate_main_dex_proguard_rules_path;
  bool generate_conditional_proguard_rules = false;
  bool generate_non_final_ids = false;
  std::vector<std::string> javadoc_annotations;
  Maybe<std::string> private_symbols;

  // Optimizations/features.
  bool no_auto_version = false;
  bool no_version_vectors = false;
  bool no_version_transitions = false;
  bool no_resource_deduping = false;
  bool no_xml_namespaces = false;
  bool do_not_compress_anything = false;
  std::unordered_set<std::string> extensions_to_not_compress;

  // Static lib options.
  bool no_static_lib_packages = false;
  bool auto_namespace_static_lib = false;

  // AndroidManifest.xml massaging options.
  ManifestFixerOptions manifest_fixer_options;

  // Products to use/filter on.
  std::unordered_set<std::string> products;

  // Flattening options.
  TableFlattenerOptions table_flattener_options;

  // Split APK options.
  TableSplitterOptions table_splitter_options;
  std::vector<SplitConstraints> split_constraints;
  std::vector<std::string> split_paths;

  // Stable ID options.
  std::unordered_map<ResourceName, ResourceId> stable_id_map;
  Maybe<std::string> resource_id_map_path;

  // When 'true', allow reserved package IDs to be used for applications. Pre-O, the platform
  // treats negative resource IDs [those with a package ID of 0x80 or higher] as invalid.
  // In order to work around this limitation, we allow the use of traditionally reserved
  // resource IDs [those between 0x02 and 0x7E].
  bool allow_reserved_package_id = false;

  // Whether we should fail on definitions of a resource with conflicting visibility.
  bool strict_visibility = false;
};

class LinkCommand : public Command {
 public:
  explicit LinkCommand(IDiagnostics* diag) : Command("link", "l"),
                                             diag_(diag) {
    SetDescription("Links resources into an apk.");
    AddRequiredFlag("-o", "Output path.", &options_.output_path);
    AddRequiredFlag("--manifest", "Path to the Android manifest to build.",
        &options_.manifest_path);
    AddOptionalFlagList("-I", "Adds an Android APK to link against.", &options_.include_paths);
    AddOptionalFlagList("-A", "An assets directory to include in the APK. These are unprocessed.",
        &options_.assets_dirs);
    AddOptionalFlagList("-R", "Compilation unit to link, using `overlay` semantics.\n"
        "The last conflicting resource given takes precedence.", &overlay_arg_list_);
    AddOptionalFlag("--package-id",
        "Specify the package ID to use for this app. Must be greater or equal to\n"
            "0x7f and can't be used with --static-lib or --shared-lib.", &package_id_);
    AddOptionalFlag("--java", "Directory in which to generate R.java.",
        &options_.generate_java_class_path);
    AddOptionalFlag("--proguard", "Output file for generated Proguard rules.",
        &options_.generate_proguard_rules_path);
    AddOptionalFlag("--proguard-main-dex",
        "Output file for generated Proguard rules for the main dex.",
        &options_.generate_main_dex_proguard_rules_path);
    AddOptionalSwitch("--proguard-conditional-keep-rules",
        "Generate conditional Proguard keep rules.",
        &options_.generate_conditional_proguard_rules);
    AddOptionalSwitch("--no-auto-version", "Disables automatic style and layout SDK versioning.",
        &options_.no_auto_version);
    AddOptionalSwitch("--no-version-vectors",
        "Disables automatic versioning of vector drawables. Use this only\n"
            "when building with vector drawable support library.",
        &options_.no_version_vectors);
    AddOptionalSwitch("--no-version-transitions",
        "Disables automatic versioning of transition resources. Use this only\n"
            "when building with transition support library.",
        &options_.no_version_transitions);
    AddOptionalSwitch("--no-resource-deduping", "Disables automatic deduping of resources with\n"
            "identical values across compatible configurations.",
        &options_.no_resource_deduping);
    AddOptionalSwitch("--enable-sparse-encoding",
        "This decreases APK size at the cost of resource retrieval performance.",
        &options_.table_flattener_options.use_sparse_entries);
    AddOptionalSwitch("-x", "Legacy flag that specifies to use the package identifier 0x01.",
        &legacy_x_flag_);
    AddOptionalSwitch("-z", "Require localization of strings marked 'suggested'.",
        &require_localization_);
    AddOptionalFlagList("-c",
        "Comma separated list of configurations to include. The default\n"
            "is all configurations.", &configs_);
    AddOptionalFlag("--preferred-density",
        "Selects the closest matching density and strips out all others.",
        &preferred_density_);
    AddOptionalFlag("--product", "Comma separated list of product names to keep", &product_list_);
    AddOptionalSwitch("--output-to-dir", "Outputs the APK contents to a directory specified by -o.",
        &options_.output_to_directory);
    AddOptionalSwitch("--no-xml-namespaces", "Removes XML namespace prefix and URI information\n"
            "from AndroidManifest.xml and XML binaries in res/*.",
        &options_.no_xml_namespaces);
    AddOptionalFlag("--min-sdk-version",
        "Default minimum SDK version to use for AndroidManifest.xml.",
        &options_.manifest_fixer_options.min_sdk_version_default);
    AddOptionalFlag("--target-sdk-version",
        "Default target SDK version to use for AndroidManifest.xml.",
        &options_.manifest_fixer_options.target_sdk_version_default);
    AddOptionalFlag("--version-code",
        "Version code (integer) to inject into the AndroidManifest.xml if none is\n"
            "present.",
        &options_.manifest_fixer_options.version_code_default);
    AddOptionalFlag("--version-name",
        "Version name to inject into the AndroidManifest.xml if none is present.",
        &options_.manifest_fixer_options.version_name_default);
    AddOptionalSwitch("--replace-version",
        "If --version-code and/or --version-name are specified, these\n"
            "values will replace any value already in the manifest. By\n"
            "default, nothing is changed if the manifest already defines\n"
            "these attributes.",
        &options_.manifest_fixer_options.replace_version);
    AddOptionalFlag("--compile-sdk-version-code",
        "Version code (integer) to inject into the AndroidManifest.xml if none is\n"
            "present.",
        &options_.manifest_fixer_options.compile_sdk_version);
    AddOptionalFlag("--compile-sdk-version-name",
        "Version name to inject into the AndroidManifest.xml if none is present.",
        &options_.manifest_fixer_options.compile_sdk_version_codename);
    AddOptionalSwitch("--shared-lib", "Generates a shared Android runtime library.",
        &shared_lib_);
    AddOptionalSwitch("--static-lib", "Generate a static Android library.", &static_lib_);
    AddOptionalSwitch("--proto-format",
        "Generates compiled resources in Protobuf format.\n"
            "Suitable as input to the bundle tool for generating an App Bundle.",
        &proto_format_);
    AddOptionalSwitch("--no-static-lib-packages",
        "Merge all library resources under the app's package.",
        &options_.no_static_lib_packages);
    AddOptionalSwitch("--auto-namespace-static-lib",
        "Automatically namespace resource references when building a static\n"
            "library.",
        &options_.auto_namespace_static_lib);
    AddOptionalSwitch("--non-final-ids",
        "Generates R.java without the final modifier. This is implied when\n"
            "--static-lib is specified.",
        &options_.generate_non_final_ids);
    AddOptionalFlag("--stable-ids", "File containing a list of name to ID mapping.",
        &stable_id_file_path_);
    AddOptionalFlag("--emit-ids",
        "Emit a file at the given path with a list of name to ID mappings,\n"
            "suitable for use with --stable-ids.",
        &options_.resource_id_map_path);
    AddOptionalFlag("--private-symbols",
        "Package name to use when generating R.java for private symbols.\n"
            "If not specified, public and private symbols will use the application's\n"
            "package name.",
        &options_.private_symbols);
    AddOptionalFlag("--custom-package", "Custom Java package under which to generate R.java.",
        &options_.custom_java_package);
    AddOptionalFlagList("--extra-packages",
        "Generate the same R.java but with different package names.",
        &extra_java_packages_);
    AddOptionalFlagList("--add-javadoc-annotation",
        "Adds a JavaDoc annotation to all generated Java classes.",
        &options_.javadoc_annotations);
    AddOptionalFlag("--output-text-symbols",
        "Generates a text file containing the resource symbols of the R class in\n"
            "the specified folder.",
        &options_.generate_text_symbols_path);
    AddOptionalSwitch("--allow-reserved-package-id",
        "Allows the use of a reserved package ID. This should on be used for\n"
            "packages with a pre-O min-sdk\n",
        &options_.allow_reserved_package_id);
    AddOptionalSwitch("--auto-add-overlay",
        "Allows the addition of new resources in overlays without\n"
            "<add-resource> tags.",
        &options_.auto_add_overlay);
    AddOptionalFlag("--rename-manifest-package", "Renames the package in AndroidManifest.xml.",
        &options_.manifest_fixer_options.rename_manifest_package);
    AddOptionalFlag("--rename-instrumentation-target-package",
        "Changes the name of the target package for instrumentation. Most useful\n"
            "when used in conjunction with --rename-manifest-package.",
        &options_.manifest_fixer_options.rename_instrumentation_target_package);
    AddOptionalFlagList("-0", "File extensions not to compress.",
        &options_.extensions_to_not_compress);
    AddOptionalSwitch("--no-compress", "Do not compress any resources.",
        &options_.do_not_compress_anything);
    AddOptionalSwitch("--warn-manifest-validation",
        "Treat manifest validation errors as warnings.",
        &options_.manifest_fixer_options.warn_validation);
    AddOptionalFlagList("--split",
        "Split resources matching a set of configs out to a Split APK.\n"
            "Syntax: path/to/output.apk:<config>[,<config>[...]].\n"
            "On Windows, use a semicolon ';' separator instead.",
        &split_args_);
    AddOptionalSwitch("-v", "Enables verbose logging.", &verbose_);
    AddOptionalSwitch("--debug-mode",
        "Inserts android:debuggable=\"true\" in to the application node of the\n"
            "manifest, making the application debuggable even on production devices.",
        &options_.manifest_fixer_options.debug_mode);
    AddOptionalSwitch("--strict-visibility",
        "Do not allow overlays with different visibility levels.",
        &options_.strict_visibility);
  }

  int Action(const std::vector<std::string>& args) override;

 private:
  IDiagnostics* diag_;
  LinkOptions options_;

  std::vector<std::string> overlay_arg_list_;
  std::vector<std::string> extra_java_packages_;
  Maybe<std::string> package_id_;
  std::vector<std::string> configs_;
  Maybe<std::string> preferred_density_;
  Maybe<std::string> product_list_;
  bool legacy_x_flag_ = false;
  bool require_localization_ = false;
  bool verbose_ = false;
  bool shared_lib_ = false;
  bool static_lib_ = false;
  bool proto_format_ = false;
  Maybe<std::string> stable_id_file_path_;
  std::vector<std::string> split_args_;
};

}// namespace aapt

#endif //AAPT2_LINK_H