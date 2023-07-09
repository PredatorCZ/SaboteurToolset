/*  SaboteurToolset
    Copyright(C) 2023 Lukas Cone

    This program is free software : you can redistribute it and / or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.If not, see <https://www.gnu.org/licenses/>.
*/

#include "hashstorage.hpp"
#include <iostream>

int main(int, char **) {
  char buffer[0x1000]{};

  while (true) {
    std::cin.getline(buffer, sizeof(buffer));
    std::cout << std::hex << hash::GetHash(buffer) << std::endl;
  }

  return 0;
}
