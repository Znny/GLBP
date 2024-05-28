openGL BluePrint/BoilerPlate project - used as a starting point for opengl projects!


todo:
    'common uniform variables' - data used across many shaders
        camera details
            transform
            lens info
        timing info
            current time
            current date
            update rate
                current
                sliding-window average
        resolution
        mouse details
        lighting information
            -ability to easily add lights
            directional lights
            point lights
            spotlights
        
    'materials'
        materials are really just shaders...
        different material categories
            forward shading - shaders that render directly to one or more buffers
            deferred shading - shaders that write data to one or more buffers, and rendered at a later time
            post-process - shaders that operate on a frameuffer

    'renderables'
        renderables consist of one or more materials 
        coupled to one or more sets of geometric primitives

    'post-processing'
        d-buffer, g buffer, etc. 
        need to look up the specifics for the above

    
