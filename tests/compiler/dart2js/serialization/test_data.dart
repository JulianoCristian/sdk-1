// Copyright (c) 2016, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

library dart2js.serialization_test_data;

const List<Test> TESTS = const <Test>[
  const Test('Empty program', const {
    'main.dart': 'main() {}'
  }),

  const Test('Hello World', const {
    'main.dart': 'main() => print("Hello World");'
  }),

  const Test('Too many arguments to print', const {
    'main.dart': 'main() => print("Hello World", 0);'
  },
  expectedWarningCount: 1,
  expectedInfoCount: 1),

  const Test('Hello World with string interpolation', const {
    'main.dart': r'''
main() {
  String text = "Hello World";
  print('$text');
}'''
  }),

  const Test('Too many arguments to print with string interpolation', const {
    'main.dart': r'''
main() {
  String text = "Hello World";
  print('$text', text);
}'''
  },
  expectedWarningCount: 1,
  expectedInfoCount: 1),

  const Test('Print main arguments', const {
    'main.dart': r'''
main(List<String> arguments) {
  print(arguments);
}'''
  }),

  const Test('For loop on main arguments', const {
      'main.dart': r'''
main(List<String> arguments) {
  for (int i = 0; i < arguments.length; i++) {
    print(arguments[i]);
  }
}'''
    }),

  const Test('For-in loop on main arguments', const {
    'main.dart': r'''
main(List<String> arguments) {
  for (String argument in arguments) {
    print(argument);
  }
}'''
  }),

  const Test('Simple class', const {
    'main.dart': r'''
class Class {}
main() {
  print(new Class());
}'''
  }),

  const Test('Simple class implements Function without call method', const {
    'main.dart': r'''
class Class implements Function {}
main() {
  print(new Class());
}'''
  },
  expectedWarningCount: 1),

  const Test('Simple class implements Function with call method', const {
    'main.dart': r'''
class Class implements Function {
  call() {}
}
main() {
  print(new Class()());
}'''
  }),

  const Test('Implement Comparable', const {
    'main.dart': r'''
class Class implements Comparable<Class> {
  int compareTo(Class other) => 0;
}
main() {
  print(new Class());
}'''
  }),

  const Test('Implement Comparable with two many type arguments', const {
    'main.dart': r'''
class Class implements Comparable<Class, Class> {
  int compareTo(other) => 0;
}
main() {
  print(new Class());
}'''
  },
  expectedWarningCount: 1),

  const Test('Impliment Comparable with incompatible parameter types', const {
    'main.dart': r'''
class Class implements Comparable<Class> {
  int compareTo(String other) => 0;
}
main() {
  print(new Class().compareTo(null));
}'''
  },
  expectedWarningCount: 1,
  expectedInfoCount: 1),

  const Test('Impliment Comparable with incompatible parameter count', const {
    'main.dart': r'''
class Class implements Comparable {
  bool compareTo(a, b) => true;
}
main() {
  print(new Class().compareTo(null, null));
}'''
  },
  expectedWarningCount: 1,
  expectedInfoCount: 1),

  const Test('Implement Random and call nextInt directly', const {
    'main.dart': r'''
import 'dart:math';

class MyRandom implements Random {
  int nextInt(int max) {
    return max.length;
  }
  bool nextBool() => true;
  double nextDouble() => 0.0;
}
main() {
  new MyRandom().nextInt(0);
}'''
  },
  expectedWarningCount: 1,
  expectedInfoCount: 0),

  const Test('Implement Random and do not call nextInt', const {
    'main.dart': r'''
import 'dart:math';

class MyRandom implements Random {
  int nextInt(int max) {
    return max.length;
  }
  bool nextBool() => true;
  double nextDouble() => 0.0;
}
main() {
  new MyRandom();
}'''
  }),

  const Test('Implement Random and call nextInt through native code', const {
    'main.dart': r'''
import 'dart:math';

class MyRandom implements Random {
  int nextInt(int max) {
    return max.length;
  }
  bool nextBool() => true;
  double nextDouble() => 0.0;
}
main() {
  // Invocation of `MyRandom.nextInt` is only detected knowing the actual 
  // implementation class for `List` and the world impact of its `shuffle` 
  // method.  
  [].shuffle(new MyRandom());
}'''
  },
  expectedWarningCount: 1,
  expectedInfoCount: 0),

  const Test('Handle break and continue', const {
    'main.dart': '''
main() {
  loop: for (var a in []) {
    for (var b in []) {
      continue loop;
    }
    break;
  }      
}'''
  }),

  const Test('Explicit default constructor', const {
    'main.dart': '''
class A {
  A();
}
main() => new A();
    ''',
  }),

  const Test('Explicit default constructor, preserialized', const {
    'main.dart': '''
import 'lib.dart';
main() => new A();
    ''',
  }, preserializedSourceFiles: const {
    'lib.dart': '''
class A {
  A();
}
''',
  }),

  const Test('Const constructor', const {
    'main.dart': '''
class C {
  const C();
}
main() => const C();'''
  }),

  const Test('Redirecting factory', const {
    'main.dart': '''
class C {
  factory C() = Object;
}
main() => new C();'''
  }),

  const Test('Redirecting factory with optional arguments', const {
    'main.dart': '''
abstract class C implements List {
  factory C([_]) = List;
}
main() => new C();'''
  }),

  const Test('Constructed constant using its default values.', const {
    'main.dart': '''
main() => const Duration();
''',
  }),

  const Test('Call forwarding constructor on named mixin application',
      const {
        'main.dart': '''
import 'dart:collection';
main() => new UnmodifiableListView(null);
''',
  }),

  const Test('Function reference constant', const {
    'main.dart': '''
var myIdentical = identical;
main() => myIdentical;
''',
  }),

  const Test('Super method call', const {
    'main.dart': '''
class Foo {
  String toString() => super.toString();
}
main() {
  print(new Foo());
}
''',
  }),

  const Test('Call forwarding constructor on named mixin application, no args.',
      const {
        'main.dart': '''
import 'lib.dart';
main() => new C();
''',
  }, preserializedSourceFiles: const {
        'lib.dart': '''
class M {}
class S {}
class C = S with M;
''',
      }),

  const Test('Call forwarding constructor on named mixin application, one arg.',
      const {
        'main.dart': '''
import 'lib.dart';
main() => new C(0);
''',
      }, preserializedSourceFiles: const {
        'lib.dart': '''
class M {}
class S {
  S(a);
}
class C = S with M;
''',
      }),

  const Test('Import mirrors, thus checking import paths', const {
    'main.dart': '''
import 'dart:mirrors';
main() {}
''',
  },
      expectedWarningCount: 1),

  const Test('Serialized symbol literal', const {
    'main.dart': '''
import 'lib.dart';
main() => m();
''',
  }, preserializedSourceFiles: const {
    'lib.dart': '''
m() => print(#main);
''',
  }),

  const Test('Indirect unserialized library', const {
    'main.dart': '''
import 'a.dart';
main() => foo();
''',
  }, preserializedSourceFiles: const {
    'a.dart': '''
import 'memory:b.dart';
foo() => bar();
''',
  }, unserializedSourceFiles: const {
    'b.dart': '''
import 'memory:a.dart';
bar() => foo();
''',
  }),

  const Test('Multiple structurally identical mixins', const {
    'main.dart': '''
class S {}
class M {}
class C1 extends S with M {}
class C2 extends S with M {}
main() {
  new C1();
  new C2();
}
''',
  }),
];

class Test {
  final String name;
  final Map sourceFiles;
  final Map preserializedSourceFiles;
  final Map unserializedSourceFiles;
  final int expectedErrorCount;
  final int expectedWarningCount;
  final int expectedHintCount;
  final int expectedInfoCount;

  const Test(
      this.name,
      this.sourceFiles,
      {this.preserializedSourceFiles,
      this.unserializedSourceFiles,
      this.expectedErrorCount: 0,
      this.expectedWarningCount: 0,
      this.expectedHintCount: 0,
      this.expectedInfoCount: 0});
}