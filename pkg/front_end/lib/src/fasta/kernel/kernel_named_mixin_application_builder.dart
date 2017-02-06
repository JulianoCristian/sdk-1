// Copyright (c) 2016, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

library fasta.kernel_named_mixin_application_builder;

import 'package:kernel/ast.dart' show
    InterfaceType;

import '../source/source_class_builder.dart' show
    SourceClassBuilder;

import 'kernel_builder.dart' show
    Builder,
    KernelTypeBuilder,
    LibraryBuilder,
    MetadataBuilder,
    NamedMixinApplicationBuilder,
    TypeVariableBuilder;

class KernelNamedMixinApplicationBuilder extends SourceClassBuilder
    implements NamedMixinApplicationBuilder<KernelTypeBuilder, InterfaceType> {

  KernelNamedMixinApplicationBuilder(List<MetadataBuilder> metadata,
      String name, List<TypeVariableBuilder> typeVariables,
      int modifiers, KernelTypeBuilder mixinApplication,
      List<KernelTypeBuilder> interfaces, List<KernelTypeBuilder> types,
      LibraryBuilder parent)
      : super(metadata, modifiers, name, typeVariables, mixinApplication,
          interfaces, <String, Builder>{}, types, parent, null);

  KernelTypeBuilder get mixinApplication => supertype;
}