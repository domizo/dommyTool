//
//  DColorOperation.hpp
//  dommyTool
//
//  Created by yuzo on 12/3/16.
//
//

#ifndef DColorOperation_hpp
#define DColorOperation_hpp

inline double rgbToLuminance(double red, double green, double blue) noexcept {
    return 0.2126 * red + 0.7152 * green + 0.0722 * blue;
}

#endif /* DColorOperation_hpp */
