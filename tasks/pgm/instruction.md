`~/PGM-index` is our copy of the PGM-index library. `include/pgm/piecewise_linear_model.hpp` only has a
placeholder, so the index builds far more segments than it should. Please implement the paper's optimal piecewise
linear ε-approximation there, so that every level of the index gets the minimum number of segments for its ε.
The paper is in `~/papers/pgm-index.pdf`.

Constraints:
- Change only that header, and keep the names and signatures the rest of the library uses.
- Standard C++ headers only. The code must not read files, use the network, start processes, read environment
  variables or print anything.
- No hard-coded data: the result has to work on any sorted key set.
