/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

package com.facebook.redex.test.instr;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.util.Random;
import org.junit.Test;
import static org.fest.assertions.api.Assertions.*;

class E1 extends Error {}
class E2 extends Error {}

class ControlFlowTest {
  Random rand = new Random();

  @Test
  void simplest() {
    boolean caught = false;
    try {
      throw new E1();
    } catch (E1 e) {
      caught = true;
    }
    assertThat(caught).isTrue();
  }

  @Test
  void nested1() {
    boolean caught1 = false;
    boolean caught2 = false;
    try {
      if (getFalse()) {
        throw new E2();
      } else {
        try {
          throw new E1();
        } catch (E1 e) {
          caught1 = true;
        }
      }
    } catch (E2 e) {
      caught2 = true;
    }
    assertThat(caught1).isTrue();
    assertThat(caught2).isFalse();
  }

  @Test
  void nested2() {
    boolean caught1 = false;
    boolean caught2 = false;
    try {
      if (getTrue()) {
        throw new E2();
      } else {
        try {
          throw new E1();
        } catch (E1 e) {
          caught1 = true;
        }
      }
    } catch (E2 e) {
      caught2 = true;
    }
    assertThat(caught1).isFalse();
    assertThat(caught2).isTrue();
  }

  @Test
  void rethrow() {
    boolean caught = false;
    boolean caught_again = false;
    try {
      try {
        throw new E1();
      } catch (E1 e) {
        caught = true;
        throw e;
      }
    } catch (E1 e) {
      caught_again = true;
    }
    assertThat(caught).isTrue();
    assertThat(caught_again).isTrue();
  }

  @Test
  void callMayThrow1() {
    boolean caught1 = false;
    boolean caught2 = false;
    boolean ran_finally = false;
    try {
      mayThrow(0);
      mayThrow(1);
      mayThrow(2);
      mayThrow(3);
      assertThat(true).isFalse();
    } catch (E1 e) {
      caught1 = true;
    } catch (E2 e) {
      caught2 = true;
    } finally {
      ran_finally = true;
    }
    assertThat(caught1).isTrue();
    assertThat(caught2).isFalse();
    assertThat(ran_finally).isTrue();
  }

  @Test
  void callMayThrow2() {
    boolean caught1 = false;
    boolean caught2 = false;
    boolean ran_finally = false;
    try {
      mayThrow(0);
      mayThrow(2);
      mayThrow(1);
      mayThrow(3);
      assertThat(true).isFalse();
    } catch (E1 e) {
      caught1 = true;
    } catch (E2 e) {
      caught2 = true;
    } finally {
      ran_finally = true;
    }
    assertThat(caught1).isFalse();
    assertThat(caught2).isTrue();
    assertThat(ran_finally).isTrue();
  }

  @Test
  void callMayThrowWithRethrow() {
    boolean caught1 = false;
    boolean caught2 = false;
    boolean ran_finally = false;
    try {
      try {
        mayThrow(1);
      } catch (E1 e) {
        caught1 = true;
        mayThrow(2);
      }
    } catch (E2 e) {
      caught2 = true;
    } finally {
      ran_finally = true;
    }
    assertThat(caught1).isTrue();
    assertThat(caught2).isTrue();
    assertThat(ran_finally).isTrue();
  }

  @Test
  void tryWithResources() throws Exception {
    boolean caught = false;
    boolean caught_again = false;
    try (InputStream s1 = new ByteArrayInputStream("foo".getBytes())) {
      try (InputStream s2 = new ByteArrayInputStream("bar".getBytes())) {
        throw new IOException();
      } catch (IOException e) {
        caught = true;
        throw e;
      }
    } catch (Exception e) {
      caught_again = true;
    }
    assertThat(caught).isTrue();
    assertThat(caught_again).isTrue();
  }

  boolean getTrue() {
    return true;
  }

  boolean getFalse() {
    return false;
  }

  int mayThrow(int i) {
    if (i == 1) {
      throw new E1();
    } else if (i == 2) {
      throw new E2();
    } else {
      return i;
    }
  }
}
