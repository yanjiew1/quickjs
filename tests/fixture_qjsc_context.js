/* The native fault unit compiles this real qjsc output and its main. */
if (typeof Object !== 'function' || typeof Date !== 'function')
    throw Error('generated context lacks its basic intrinsics');
if (typeof Promise !== 'function' || typeof JSON !== 'object')
    throw Error('generated context lacks selected features');
