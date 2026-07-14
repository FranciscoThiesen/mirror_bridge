#!/usr/bin/env python3
"""
Test string type support in mirror_bridge.
Tests std::string and std::string_view bindings.
"""

import sys
import os

# Add build directory to path
sys.path.append(os.path.join(os.path.dirname(__file__), 'build'))

try:
    import document
except ImportError as e:
    print(f"❌ Failed to import document: {e}")
    print("Make sure to build the bindings first with ../build_bindings.sh")
    sys.exit(1)

def test_document_strings():
    """Test Document class with different string types"""
    print("=== Mirror Bridge String Types Test ===\n")

    # Test 1: Create Document and check default values
    print("Test 1: Creating Document with default values...")
    doc = document.Document()
    print(f"  ✓ Created Document")
    print(f"  title: '{doc.title}'")
    print(f"  content: '{doc.content}'")
    print(f"  preview: '{doc.preview}'")
    print(f"  word_count: {doc.word_count}")
    assert doc.title == "", "Default title should be empty"
    assert doc.content == "", "Default content should be empty"
    assert doc.word_count == 0, "Default word_count should be 0"
    print()

    # Test 2: Modify std::string fields
    print("Test 2: Modifying std::string fields...")
    doc.title = "Mirror Bridge Documentation"
    doc.content = "This is a test document for the mirror bridge library."
    print(f"  Set title: '{doc.title}'")
    print(f"  Set content: '{doc.content[:30]}...'")
    assert doc.title == "Mirror Bridge Documentation", "Title should be updated"
    assert "mirror bridge" in doc.content, "Content should be updated"
    print(f"  ✓ std::string fields work correctly")
    print()

    # Test 3: Modify string_view field
    print("Test 3: Modifying std::string_view field...")
    doc.preview = "Preview text"
    print(f"  Set preview: '{doc.preview}'")
    assert doc.preview == "Preview text", "Preview should be updated"
    print(f"  ✓ std::string_view fields work correctly")
    print(f"  ⚠  Note: string_view lifetime should be managed carefully in C++")
    print()

    # Test 4: Unicode and special characters
    print("Test 4: Unicode and special characters...")
    doc.title = "Tëst Dõcumént 文档 📝"
    print(f"  Set title with unicode: '{doc.title}'")
    assert "文档" in doc.title, "Unicode should be preserved"
    assert "📝" in doc.title, "Emoji should be preserved"
    print(f"  ✓ Unicode strings work correctly")
    print()

    # Test 5: Empty and long strings
    print("Test 5: Empty and long strings...")
    doc.content = ""
    assert doc.content == "", "Empty string should work"
    long_text = "A" * 10000
    doc.content = long_text
    assert len(doc.content) == 10000, "Long strings should work"
    print(f"  ✓ Empty string works")
    print(f"  ✓ Long string (10000 chars) works")
    print()

    # Test 6: Multiple instances
    print("Test 6: Multiple independent instances...")
    doc1 = document.Document()
    doc2 = document.Document()
    doc1.title = "Document 1"
    doc2.title = "Document 2"
    print(f"  doc1.title = '{doc1.title}'")
    print(f"  doc2.title = '{doc2.title}'")
    assert doc1.title != doc2.title, "Instances should be independent"
    print(f"  ✓ Multiple instances work independently")
    print()

    print("========================================")
    print("✓ All string types tests passed!")
    print("========================================")

if __name__ == "__main__":
    test_document_strings()
