Safe Configuration Management
=============================

Contributed by ConfiguredThings Ltd

All system rely on some form of configuration data, and the configuration interface is a significant part of the attack surface.
Misconfiguration, whether accidental or malicious, is one of the main sources of security vulnerabilities.
Purely immutable systems, where configuration is baked in at build time, may work for container based environments where re-deployment is relatively easy but are not an option for embedded systems.  
The solution in many cases is to create an often complex trust model around and within the configuration system to limit and control configuration changes, where the complexity itself adds to the risk profile. 

This example shows how the CHERIoT features can be used to create a system where configuration data can be received from an external source and securely and safely distributed to a number compartments with a simple and minimal trust model.
This supports, for example, an architecture where third party components can be safely integrated and dynamically configured.

There are three main aspects to the security model:
* Static Sealed Capabilities are used to define which compartments can modify and/or consume configuration data, and how much memory each configuration item can consume.  
As well as providing run time controls this aspect of the configuration can be audited at build time.

* A configuration broker provides an abstraction layer between providers and consumers of configuration data, enabling the trust model to be expressed only in terms of the relationships with the broker.

* A sandbox compartment in which untrusted data can be verified and converted to trusted configuration values.

In the context of this example the Configuration Broker is intended to be directly reusable, whilst the providers and consumers are example code of how to develop such components using CHERIoT features such as static sealed capabilities, memory claims, locks, event-waiters, and sandbox compartments for handling untrusted data.

Providing a generic broker and expressing the trust model via its interfaces makes it possible to add support for new configuration items without having to re-evaluate the trust model each time. 

## Overview

Each item of configuration data has a name, a value, and a version.
New values are supplied as serialised JSON and parsed into a corresponding data strut. 

There are four main roles:
* Providers are authorised to supply the values for one or more items.
* Consumers are authorised to receive the values of one or more items.
* Parsers are authorised to register the method which parses new values from Providers into verified values for Consumers. 
* The Broker provides an abstraction layer between Providers and Consumers; it holds the current value and maintains the version for each item.
The Broker is passive, in that it only responds to calls from a Provider or Consumer.
All operations take place on a thread calling into the Broker, and it only calls out to the Parser.

Providers, Consumers, and Parsers are assigned their rights via Static Sealed Capabilities, which only the Broker can unseal.
This means this aspect is fixed at build time and is auditable.

<img src=doc/broker.svg>

The model can be thought of as similar to a pub/sub architecture with a single retained message for each item and a security policy defined at build time through static sealed capabilities.
The configuration data is declarative so there is no need or value in maintaining a full sequence of updates; each new version is a complete definition of the required configuration item. 
Aligned with the pub/sub model publishing items and subscribing for items can happen in any sequence; a consumer will receive any data that is already published, and any subsequent updates.
This avoids any timing issues during system startup.

In the demo new values are provided as serialised JSON strings, with each item coming from a specific topic from a (not included) MQTT Broker.
The Provider is in effect a simply an authorised mapping between the topics and configuration items.
The Parser and Consumers contain code which is specific to each item.
The Broker is agnostic to the details of configuration items, and has no prior knowledge of which items exist.

### Interactions and Trust Model
Because the Broker provides an abstraction between Providers, Consumers and Parsers all of the interactions can be described in terms of their interactions with the Broker rather than each other.

#### Parsers
The parsers have the key role of converting untrusted data received from the network into verified and trusted configuration values.
In traditional systems parsers are vulnerable to a range of attacks such as injection and buffer overflow.
Using Cheriot each parser runs as stateless method (using heap controls) in a sandbox compartment that ensure that any issues are contained to failing only the current parse operation.
Parsers are given a static sealed capability for each item type they are allowed to parser, which includes two properties of the item.
* The size of the object they will produce.
* The minimum interval in mS between updates.
They use this to register with the Broker, which is the only compartment that can unseal the capability.
The Broker will only call registered parsers, which are passed as cheri_callbacks so they are not callable by any other compartment.
The Broker only passes non global capabilities to the Parser, so the Parser is unable to capture them.

The Broker uses the size in the sealed capability to allocate a new buffer for each update.
If the parsing of the new value results in access beyond this size then that will trigger a bounds violation that fails the parse. 

The interval reflects that parsing an object and/or applying updates can can be expensive tasks, and protects against DoS attacks from a compromised Provider.
The Broker will reject without attempting to parse any updates that are made less that min_interval since the last attempt. 

##### Integrity
The Broker trusts that the Parser will correctly populate the object, but this can be established by code inspection & testing.
Because the Parser can only be invoked by the Broker and is stateless it's correct operation can not be compromised. 

The Parser has no need for trust in the Broker; if it is passed the wrong size buffer the operation will simply fail, or result in a partly populated object.

##### Confidentiality
The Broker ensures that Parser can not capture the capabilities passed as parameters.

The Parser is not allowed to persist state in the heap. 

##### Availability
The Broker trusts that the Parser will not block only to the extent that it provides this guarantee to the Provider;  The Broker itself is still able to serve other configuration items.

#### Providers
Providers have one or more WRITE_CONFIG_CAPABILTY(s) that define the name of each item they are allowed to update. They request the broker to update the value of an item by passing it
* The sealed capability granting permission to update the item.
* A read-only string of serialised JSON.

Assuming the capability is valid, the Broker will allocate the required space from the heap (defined by the Parser and not the Publisher) and invoke the Parser.

If the parse is successful the Broker will notify any consumers by updating the version.

##### Confidentiality
The Publisher is trusting the Broker will only make the data available to compartments that have the corresponding sealed read capability.
This can be verified by code inspection and auditing the static sealed capabilities.

##### Integrity
Only a Provider with the corresponding sealed capability can request an update. 

The data object is created from a read only JSON string, and stored in heap space allocated by the Broker.
There is no path for the publisher to affect the integrity of the data after it has been successfully parsed.   

The Broker only maintains a read only pointer to the parsed data, so neither it nor a consumer can mutate it.

##### Availability
The Provider can not make the Broker consume more of its heap that the 2x the size defined in the corresponding sealed capability of the Parser (current version + new version).

The Provider can not make the Broker attempt to parse it's data more often that the minimum interval defined in the corresponding sealed capability of the Parser.

The Provider is trusting the Broker, and indirectly the Parser, not to block its thread.


#### Consumers
Consumers have one or more READ_CONFIG_CAPABILITY(s) that define which items they are allowed to receive.

Consumers can request the current value at any time by passing their sealed capability to the Broker.
In return they receive a struct with four values:
* The name of the item. 
* A read only pointer the current data value (which maybe null if it hasn't been set yet).
* The current version.
* A read only pointer to a futex they can wait on for the version to change.

The normal pattern for a consumer is to have a thread which makes an initial call to get as a minimum the current version and futex to wait on, process the current value (if any) and then wait for changes. 

The Broker allocate heap space for each new version of the data, which it releases when a new value becomes available.
Consumers must assert their own claims and fast_claims to keep the value available to them for as long as they need it. 

As with the Provider the extent to which the Broker trusts a Consumer is encapsulated in the sealed capability, so it is only "trusting" something which can be audited at build time.

##### Confidentiality
Only a Consumer with the sealed capability can request an item.

##### Integrity
The consumer is only given a read capability to the item, so it can not mutate it.

The Broker will only ever give the Consumer values that have been successfully parsed.

##### Availability
The Consumer is trusting that the Broker wil notify it when new versions are available. 

The Consumer can not affect the Brokers heap quota; if the Consumer fails to make or release a claim it only affects itself.

The Consumer is trusting that Broker will not block it's thread when it reads a value.
It has control over when it's thread waits on the futex for a new version, and for how long to wait. 

## Data in the demo

The demo uses three configuration values; two based on Sonata board and a third more contrived for the demo.
The values are mix of strings, numbers, and enumerations.  

### RGB LEDs
Sets the colour of the two RGB LEDs
```json
{
    "led0": {"red": 100, "green": 100, "blue": 100},
    "led1": {"red": 200, "green": 200, "blue": 200},
}
```
Values must be 0-255

### User LEDs
Sets the state of the eight User LEDs 
```json
{
  "led0": "on",
  "led1": "off",
  "led2": "ON",
  "led3": "OFF",
  "led4": "Off",
  "led5": "On",
  "led6": "off",
  "led7": "On"
}
```
Values are not case sensitive.

### Logger
A contrived example to include a string (to show buffer overflow handling) and which has multiple consumers.
```json
{
  "host": {
    "address": "10.100.0.30",
    "port": 514
   },
   "level": "info"
}
```

A thread which starts in the MQTT stub provides a sequence of valid and invalid configuration values from the corresponding topics.

A single Provider act as the MQTT client and maps the messages against its set of sealed capabilities based on the topic.   

There are two Consumers in the demo, each implemented as separate compartments.

Consumer #1 is authorised to receive the RGD LED configuration.
Consumer #2 is authorised to receive the User LED configuration.
Both consumers are authorised to receive the Logger configuration.  

A thread is stared in each consumer which waits for new versions to become available and then, to keep the demo h/w agnostic, makes a library call to print the received value.

The demo uses the "ibex-safe-simulator" board as its target, since this provides a realtime clock.
This allows the Provider to sleep between messages giving the Consumers a chance to run.    



