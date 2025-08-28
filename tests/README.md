# ESE Test Suite

This directory contains comprehensive unit tests for the ESE (Entity Sprite Engine) library.

## Overview

The test suite covers the core types and functionality of the ESE library, with a focus on:
- **EsePoint**: 2D point with coordinates and watcher system
- **EseRect**: 2D rectangle with position, dimensions, rotation, and collision detection

## Test Structure

### Test Utilities (`test_utils.h`)
- Common testing macros and assertions
- Test result tracking and reporting
- Mock Lua engine for testing
- Test suite management functions

### Point Tests (`test_point.c`)
- **Creation Tests**: Basic object creation and initialization
- **Properties Tests**: Setting/getting x, y coordinates
- **Copy Tests**: Deep copying functionality
- **Mathematical Operations**: Distance calculations between points
- **Watcher System**: Property change notification callbacks
- **Lua Integration**: Reference counting and Lua state management

### Rect Tests (`test_rect.c`)
- **Creation Tests**: Basic object creation and initialization
- **Properties Tests**: Setting/getting position, dimensions, and rotation
- **Copy Tests**: Deep copying functionality
- **Mathematical Operations**: Area calculations
- **Collision Detection**: Rectangle intersection and point containment
- **Watcher System**: Property change notification callbacks
- **Lua Integration**: Reference counting and Lua state management

## Building and Running Tests

### Prerequisites
- CMake 3.16 or higher
- C compiler (GCC, Clang, or MSVC)
- ESE library built and available

### Build Commands
```bash
# From the project root
mkdir -p build
cd build
cmake ..
cmake --build .

# Build and run tests
cmake --build . --target test_rect
cmake --build . --target test_point

# Run all tests
ctest --verbose
```

### Running Individual Tests
```bash
# Run point tests
./tests/test_point

# Run rect tests
./tests/test_rect
```

## Test Output

Tests provide detailed output including:
- ✓ PASS: Successful test assertions
- ✗ FAIL: Failed test assertions with expected vs actual values
- Test suite summaries with pass/fail counts
- Success rate percentages

Example output:
```
🧪 Starting EsePoint Unit Tests

=== Point Creation Tests ===
✓ PASS: point_create should return non-NULL pointer
✓ PASS: New point should have x = 0.0
✓ PASS: New point should have y = 0.0

--- Point Creation Tests Results ---
Total tests: 3
Passed: 3
Failed: 0
Success rate: 100.0%
🎉 All tests passed!
```

## Adding New Tests

To add tests for new functionality:

1. **Create test file**: `test_[feature].c`
2. **Include test utilities**: `#include "test_utils.h"`
3. **Define test functions**: Use descriptive names like `test_[feature]_[aspect]()`
4. **Use test macros**: 
   - `TEST_ASSERT(condition, message)` for boolean assertions
   - `TEST_ASSERT_EQUAL(expected, actual, message)` for equality
   - `TEST_ASSERT_FLOAT_EQUAL(expected, actual, tolerance, message)` for floating point
   - `TEST_ASSERT_NOT_NULL(ptr, message)` for pointer validation
5. **Update CMakeLists.txt**: Add new test executable
6. **Run tests**: Verify all tests pass

## Test Coverage

The test suite covers:
- ✅ **Happy Path**: Normal operation scenarios
- ✅ **Edge Cases**: Boundary conditions and limits
- ✅ **Error Handling**: NULL pointer handling, invalid inputs
- ✅ **Memory Management**: Creation, copying, destruction
- ✅ **Integration**: Lua state management and reference counting
- ✅ **Watcher System**: Property change notifications
- ✅ **Mathematical Operations**: Calculations and algorithms

## Continuous Integration

Tests are designed to run in CI/CD pipelines:
- Exit codes: 0 for success, non-zero for failure
- No external dependencies beyond the ESE library
- Fast execution for quick feedback
- Clear error reporting for debugging

## Contributing

When adding new features to ESE:
1. Write tests first (TDD approach)
2. Ensure all existing tests pass
3. Add tests for new functionality
4. Update this README if needed
5. Run the full test suite before submitting

## Troubleshooting

### Common Issues
- **Memory Manager Not Initialized**: Ensure `memory_manager_init()` is called before tests
- **Lua State Errors**: Mock engine provides NULL runtime for testing
- **Build Failures**: Check that ESE library is built and linked correctly

### Debug Mode
For detailed debugging, tests can be compiled with debug flags:
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

### Valgrind Support
Tests are compatible with memory checking tools:
```bash
valgrind --leak-check=full ./tests/test_point
valgrind --leak-check=full ./tests/test_rect
```
