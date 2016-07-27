// Copyright (c) 2016, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file

part of models;

enum ErrorKind {
  /// The isolate has encountered an unhandled Dart exception.
  UnhandledException,
  /// The isolate has encountered a Dart language error in the program.
  LanguageError,
  /// The isolate has encounted an internal error. These errors should be
  /// reported as bugs.
  InternalError,
  /// The isolate has been terminated by an external source.
  TerminationError
}

abstract class ErrorRef extends ObjectRef {
  String get id;
  ErrorKind get kind;
  String get message;
}

abstract class Error extends Object implements ErrorRef {

}