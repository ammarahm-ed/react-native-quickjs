/* eslint-disable react-native/no-inline-styles */
import React, { useEffect } from 'react';
import { Text, View, NativeModules } from 'react-native';

const App = () => {
  useEffect(() => {
    NativeModules.TTIModule.componentDidMount(Date.now());
  }, []);
  return (
    <View
      style={{
        width: '100%',
        height: '100%',
        justifyContent: 'center',
        alignItems: 'center',
      }}
      nativeID="my-test-view"
      id="my-test-view"
      collapsable={true}
      focusable={true}
    >
      <Text
        style={{
          color: 'green',
          fontWeight: 'bold',
        }}
      >
        Hello World
      </Text>
    </View>
  );
};

export default App;
