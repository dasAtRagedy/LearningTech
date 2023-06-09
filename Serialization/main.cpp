#include <iostream>
#include <vector>
#include <bitset>
#include <assert.h>
#include <fstream>
#include <random>
#include <string.h>

#define PRINT(a) result = #a;

namespace Core {
    // writing info into buffer vector in little-endian serialization
    template<typename T>
    void encode(std::vector<int8_t>* buffer, int16_t* iterator, T value) {
        for (unsigned i = 0, j = 0; i < sizeof(T); i++) {
            (*buffer)[(*iterator)++] = (value >> ((sizeof(T)* 8) - 8) - ((i == 0) ? j : j += 8));
        }
    }

    template<>
    void encode<float>(std::vector<int8_t>* buffer, int16_t* iterator, float value) {
        int32_t result = *reinterpret_cast<int32_t*>(&value);
        encode<int32_t>(buffer, iterator, result);
    }

    template<>
    void encode<double>(std::vector<int8_t>* buffer, int16_t* iterator, double value) {
        int64_t result = *reinterpret_cast<int64_t*>(&value);
        encode<int64_t>(buffer, iterator, result);
    }

    template<>
    void encode<std::string>(std::vector<int8_t>* buffer, int16_t* iterator, std::string value) {
        for (unsigned i = 0; i < value.size(); i++) {
            encode<int8_t>(buffer, iterator, value[i]);
        }
    }

    template<typename T>
    void encode(std::vector<int8_t>* buffer, int16_t* iterator, std::vector<T> value) {
        for (unsigned i = 0; i < value.size(); i++) {
            encode<T>(buffer, iterator, value[i]);
        }
    }
}

namespace ObjectModel {
    enum class Wrapper : int8_t {
        PRIMITIVE = 1,
        ARRAY,
        STRING,
        OBJECT
    };

    enum class Type : int8_t {
        I8 = 1,
        I16,
        I32,
        I64,

        U8 = 1,
        U16,
        U32,
        U64,

        FLOAT,
        DOUBLE,

        BOOL
    };

    class Root {
    protected:
        std::string name;
        int16_t nameLength;
        int8_t wrapper;
        int32_t size;
    protected:
        Root();
    public:
        int32_t getSize();
        void setName(std::string);
        std::string getName();
        //since it's a virtual method, class takes up 4 bytes more with the reference to `vtable`
        virtual void pack(std::vector<int8_t>*, int16_t*);

    };

    class Primitive : public Root {
    private:
        int8_t type = 0;
        std::vector<int8_t>* data = nullptr;
    private:
        Primitive();
    public:
        template<typename T>
        static Primitive* create(std::string name, Type type, T value) {
            Primitive* p = new Primitive();
            p->setName(name);
            p->wrapper = static_cast<int8_t>(Wrapper::PRIMITIVE);
            p->type = static_cast<int8_t>(type);
            p->data = new std::vector<int8_t>(sizeof value);
            p->size += p->data->size();
            int16_t iterator = 0;
            Core::template encode<T>(p->data, &iterator, value);

            return p;
        }

        void pack(std::vector<int8_t>*, int16_t*);
    };

    class Array : public Root {
    private:
        int8_t type = 0;
        int32_t count = 0;
        std::vector<int8_t>* data = nullptr;
    public:
        Array();
    public:
        template<typename T>
        static Array* createArray(std::string name, Type type, std::vector<T> value) {
            Array* arr = new Array();
            arr->setName(name);
            arr->wrapper = static_cast<int8_t>(Wrapper::ARRAY);
            arr->type = static_cast<int8_t>(type);
            arr->count = value.size();
            arr->data = new std::vector<int8_t>(value.size() * sizeof(T));
            arr->size += value.size() * sizeof(T);
            int16_t iterator = 0;
            Core::template encode<T>(arr->data, &iterator, value);

            return arr;
        }

        template<typename T>
        static Array* createString(std::string name, Type type, T value) {
            Array* str = new Array();
            str->setName(name);
            str->wrapper = static_cast<int8_t>(Wrapper::STRING);
            str->type = static_cast<int8_t>(type);
            str->count = value.size();
            str->data = new std::vector<int8_t>(value.size());
            str->size += value.size();
            int16_t iterator = 0;
            Core::template encode<T>(str->data, &iterator, value);

            return str;
        }

        void pack(std::vector<int8_t>*, int16_t*);
    };

    class Object : public Root {
    private:
        std::vector<Root*> entities;
        int16_t count = 0;

    public:
        Object(std::string);
        void addEntity(Root* r);
        Root* findByName(std::string);
        void pack(std::vector<int8_t>*, int16_t*);
    };
}

namespace Core {
    namespace Util {
        bool isLittleEndian() {
            int8_t a = 5; // 0x00 0x00 0x00 0x05 - desired little endian
            std::string result = std::bitset<8>(a).to_string();
            if(result.back() == '1') return true;
            return false;
        }

        void save(const char* file, std::vector<int8_t> buffer) {
            std::ofstream out;
            out.open(file);

            for(unsigned i = 0; i < buffer.size(); i++) {
                out << buffer[i];
            }

            out.close();
        }

        void retrieveNsave(ObjectModel::Root* r){
            int16_t iterator = 0;
            std::vector<int8_t> buffer(r->getSize());
            std::string name = r->getName().substr(0, r->getName().length()).append(".ser");
            r->pack(&buffer, &iterator);
            save(name.c_str(), buffer);
        }
    }

}

namespace ObjectModel {
    //definition
    Root::Root() // init list is considered best practice
            :
            name("unknown"),
            wrapper(0),
            nameLength(0),
            size(sizeof nameLength + sizeof wrapper + sizeof size) {}

    void Root::setName(std::string name) {
        this->name = name;
        nameLength = (int16_t)name.length();
        size += nameLength;
    }

    int32_t Root::getSize() {
        return size;
    }

    std::string Root::getName() {
        return name;
    }

    void Root::pack(std::vector<int8_t>* buffer, int16_t* iterator) {
        // TODO:: likely pure virtual?
    }

    Primitive::Primitive() {
        size += sizeof type;
    }

    void Primitive::pack(std::vector<int8_t>* buffer, int16_t* iterator) {
        Core::encode<std::string>(buffer, iterator, name);
        Core::encode<int16_t>(buffer, iterator, nameLength);
        Core::encode<int8_t>(buffer, iterator, wrapper);
        Core::encode<int8_t>(buffer, iterator, type);
        Core::encode<int8_t>(buffer, iterator, *data);
        Core::encode<int32_t>(buffer, iterator, size);
    }

    Array::Array() {
        size += (sizeof type) + (sizeof count);
    }

    void Array::pack(std::vector<int8_t>* buffer, int16_t* iterator) {
        Core::encode<std::string>(buffer, iterator, name);
        Core::encode<int16_t>(buffer, iterator, nameLength);
        Core::encode<int8_t>(buffer, iterator, wrapper);
        Core::encode<int8_t>(buffer, iterator, type);
        Core::encode<int32_t>(buffer, iterator, count);
        Core::encode<int8_t>(buffer, iterator, *data);
        Core::encode<int32_t>(buffer, iterator, size);
    }

    Object::Object(std::string name) {
        setName(name);
        wrapper = static_cast<int8_t>(Wrapper::OBJECT);
        size += sizeof count;
    }

    void Object::addEntity(Root* r) {
        this->entities.push_back(r);
        count += 1;
        size += r->getSize();
    }

    Root* Object::findByName(std::string name) {
        for(auto r: entities) {
            if(r->getName() == name) {
                return r;
            }
        }
        std::cout<<"Not found.\n";
        return new Object("dummy");
    }

    void Object::pack(std::vector<int8_t>* buffer, int16_t* iterator) {
        Core::encode<std::string>(buffer, iterator, name);
        Core::encode<int16_t>(buffer, iterator, nameLength);
        Core::encode<int8_t>(buffer, iterator, wrapper);
        Core::encode<int16_t>(buffer, iterator, count);

        for (Root* r : entities) {
            r->pack(buffer, iterator);
        }

        Core::encode<int32_t>(buffer, iterator, size);
    }
}

namespace EventSystem {
    class Event;

    class System {
    private:
        friend class Event;
        std::string name;
        int32_t descriptor;
        int16_t index;
        bool active;
        std::vector<Event *> events;

    public:
        System(std::string);
        ~System();
    public:
        void addEvent(Event *);
        Event *getEvent();
        void serialize();
    };

    class Event {
    private:
        int32_t id;
    public:
        enum DeviceType : int8_t {
            KEYBOARD = 1,
            MOUSE,
            TOUCHPAD,
            JOYSTICK
        };
        DeviceType dType;
        System *system = nullptr;
    public:
        Event(DeviceType);
        DeviceType getdType();
        int32_t getID();
        friend std::ostream& operator<<(std::ostream& stream, const DeviceType dType) {
            std::string result;
            switch (dType) {
                case KEYBOARD: PRINT(KEYBOARD); break;
                case MOUSE: PRINT(MOUSE); break;
                case TOUCHPAD: PRINT(TOUCHPAD); break;
                case JOYSTICK: PRINT(JOYSTICK); break;
            }
            return stream << result;
        }
        void bind(System*, Event*);
        void serialize(ObjectModel::Object* o);
    };

    class KeyboardEvent : public Event {
    private:
        int16_t keyCode;
        bool pressed;
        bool released;
    public:
        KeyboardEvent(int16_t, bool, bool);
        void serialize(ObjectModel::Object* o);
    };

    //definition
    System::System(std::string name)
        :
        name(name),
        descriptor(123),
        index(1),
        active(true) {}

    System::~System() {
        //TODO::
    }

    void System::addEvent(Event* e) {
        e->bind(this, e);
    }

    Event* System::getEvent() {
        return events.front();
    }

    int32_t Event::getID() {
        return id;
    }

    /*
        friend class Event;
        std::string name;
        int32_t descriptor;
        int16_t index;
        bool active;
        std::vector<Event *> events;
     */

    void System::serialize(){
        ObjectModel::Object system("SysInfo");
        ObjectModel::Array* name = ObjectModel::Array::createString("sysname", ObjectModel::Type::I8, this->name);
        ObjectModel::Primitive* desc = ObjectModel::Primitive::create("desc", ObjectModel::Type::I32, this->descriptor);
        ObjectModel::Primitive* index = ObjectModel::Primitive::create("index", ObjectModel::Type::I16, this->index);
        ObjectModel::Primitive* active = ObjectModel::Primitive::create("active", ObjectModel::Type::BOOL, this->active);
        system.addEntity(name);
        system.addEntity(desc);
        system.addEntity(index);
        system.addEntity(active);

        for (Event* e : events) {
            KeyboardEvent* kb = static_cast<KeyboardEvent*>(e);
            ObjectModel::Object* eventObject = new ObjectModel::Object("Event: " + std::to_string(e->getID()));
            kb->serialize(eventObject);
            system.addEntity(eventObject);
        }

        Core::Util::retrieveNsave(&system);
    }

    Event::Event(DeviceType dType) {
        std::random_device rd;
        std::uniform_int_distribution<> distribution(1, 1000); //
        this->id = distribution(rd);
        this->dType = dType;
    }

    void Event::bind(System* system, Event* e) {
        this->system = system;
        this->system->events.push_back(e);
    }

    void Event::serialize(ObjectModel::Object* o) {
        ObjectModel::Primitive* id = ObjectModel::Primitive::create("id", ObjectModel::Type::I32, this->getID());
        ObjectModel::Primitive* dType = ObjectModel::Primitive::create("dType", ObjectModel::Type::I8, static_cast<int8_t>(this->dType));

        o->addEntity(id);
        o->addEntity(dType);
    }

    Event::DeviceType Event::getdType() {
        return this->dType;
    }

    KeyboardEvent::KeyboardEvent(int16_t keyCode, bool pressed, bool released)
        :
        Event(Event::KEYBOARD),
        keyCode(keyCode),
        pressed(pressed),
        released(released) {}

    void KeyboardEvent::serialize(ObjectModel::Object* o) {
        Event::serialize(o);
        ObjectModel::Primitive* keyCode = ObjectModel::Primitive::create("keyCode", ObjectModel::Type::I32, this->keyCode);
        ObjectModel::Primitive* pressed = ObjectModel::Primitive::create("pressed", ObjectModel::Type::BOOL, this->pressed);
        ObjectModel::Primitive* released = ObjectModel::Primitive::create("released", ObjectModel::Type::BOOL, this->released);
        o->addEntity(keyCode);
        o->addEntity(pressed);
        o->addEntity(released);
    }
}

using namespace EventSystem;
using namespace ObjectModel;
using namespace Core;

int main(int argc, char **argv) {
    assert(Core::Util::isLittleEndian());

#if 0
    std::vector<int64_t> data{1, 2, 3, 4};
    Array* arr = Array::createArray("array", Type::I64, data);
    Core::Util::retrieveNsave(arr);

    std::string name = "name";
    Array* str = Array::createString("string", Type::I8, name);
    Core::Util::retrieveNsave(str);

    int32_t foo = 5;
    Primitive* p = Primitive::create("int32", Type::I32, foo);
    Core::Util::retrieveNsave(p);


    Object Test("Test");
    Test.addEntity(p);
    Test.addEntity(arr);
    Test.addEntity(str);

    Object Test2("Test2");
    Test2.addEntity(p);
    Core::Util::retrieveNsave(&Test2);

    Test.addEntity(&Test2);
    Core::Util::retrieveNsave(&Test);

#endif

#if 1
    System Foo("Foo");
    Event* e = new KeyboardEvent('a', true, false);

    Foo.addEvent(e);
    KeyboardEvent* kb = static_cast<KeyboardEvent*>(Foo.getEvent());

    Foo.serialize();
#endif

    (void) argc;
    (void) argv;
    return 0;
}
