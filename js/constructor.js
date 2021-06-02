print(Whatever);

function Wrapper(ctor) {
  return function(...args) {
    return ctor.apply(this, args);
  }
}

const WrappedWhatever = Wrapper(Whatever);

const obj = new WrappedWhatever();

print(obj);
print(obj instanceof WrappedWhatever);
print(obj.prop);
print(obj.constructor.name);
